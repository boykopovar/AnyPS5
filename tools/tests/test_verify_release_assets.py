import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "verify_release_assets.py"


def release_asset(name, data):
    return {
        "apiUrl": "https://api.github.com/repos/example/project/releases/assets/123",
        "contentType": "application/zip",
        "createdAt": "2026-11-11T15:00:39Z",
        "digest": "sha256:" + hashlib.sha256(data).hexdigest(),
        "downloadCount": 0,
        "id": "RA_fixture",
        "label": "",
        "name": name,
        "size": len(data),
        "state": "uploaded",
        "updatedAt": "2026-11-11T15:00:39Z",
        "url": f"https://github.com/example/project/releases/download/v1/{name}",
    }


class VerifyReleaseAssetsTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.package = self.root / "prx-linux-v1.zip"
        self.package.write_bytes(b"new package")
        self.notes = self.root / "release-notes.md"
        self.notes.write_bytes(b"release notes\n")
        self.paths = [self.package, self.notes]
        self.assets = [release_asset(path.name, path.read_bytes()) for path in self.paths]

    def verify(self, assets=None, paths=None, raw=None):
        return subprocess.run(
            [sys.executable, str(SCRIPT), *(self.paths if paths is None else paths)],
            input=json.dumps({"assets": self.assets if assets is None else assets}).encode() if raw is None else raw,
            capture_output=True,
        )

    def test_matches_gh_release_view_assets_in_any_order(self):
        result = self.verify(assets=self.assets[::-1])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b"Verified 2 release assets", result.stdout)
        self.assertEqual(result.stderr, b"")

    def test_same_named_same_sized_corrupt_upload_is_rejected(self):
        assets = [release_asset(self.package.name, b"old package"), self.assets[1]]
        self.assertEqual(sorted(asset["name"] for asset in assets), sorted(path.name for path in self.paths))
        self.assertEqual(assets[0]["size"], self.package.stat().st_size)
        result = self.verify(assets=assets)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn(b"prx-linux-v1.zip", result.stderr)
        self.assertIn(b"SHA-256 mismatch", result.stderr)

    def test_changed_sizes_report_both_upload_and_local_lengths(self):
        result = self.verify(assets=[release_asset(self.package.name, b"short"), self.assets[1]])
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn(b"prx-linux-v1.zip", result.stderr)
        self.assertIn(b"size mismatch", result.stderr)
        self.assertIn(b"11 bytes", result.stderr)
        self.assertIn(b"5 bytes", result.stderr)

    def test_missing_and_unexpected_assets_are_reported_together(self):
        result = self.verify(assets=[release_asset("stale-v1.zip", b"stale"), self.assets[1]])
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn(b"Missing assets: 'prx-linux-v1.zip'", result.stderr)
        self.assertIn(b"Unexpected assets: 'stale-v1.zip'", result.stderr)

    def test_missing_digest_invalid_size_and_unfinished_assets_fail_closed(self):
        cases = (
            ("digest", None, b"missing or invalid SHA-256 digest"),
            ("digest", "sha256:1234", b"missing or invalid SHA-256 digest"),
            ("size", True, b"missing or invalid size"),
            ("size", -1, b"missing or invalid size"),
            ("state", "starter", b"not uploaded"),
        )
        for field, value, error in cases:
            with self.subTest(field=field, value=value):
                result = self.verify(assets=[{**self.assets[0], field: value}, self.assets[1]])
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertIn(error, result.stderr)

    def test_bad_payload_and_duplicate_names_are_rejected(self):
        for raw in (b"", b"{}", b'{"assets": null}', b'{"assets": [null]}'):
            with self.subTest(raw=raw):
                result = self.verify(raw=raw)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertNotIn(b"Traceback", result.stderr)
        result = self.verify(assets=[*self.assets, self.assets[0]])
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn(b"Duplicate release asset name: 'prx-linux-v1.zip'", result.stderr)
        result = self.verify(paths=[*self.paths, self.package])
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn(b"Duplicate local asset name: 'prx-linux-v1.zip'", result.stderr)

    def test_missing_local_file_is_rejected_even_for_matching_names(self):
        self.package.unlink()
        result = self.verify()
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn(b"prx-linux-v1.zip", result.stderr)
        self.assertIn(b"not a file", result.stderr)


if __name__ == "__main__":
    unittest.main()
