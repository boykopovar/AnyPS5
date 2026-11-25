import hashlib
import io
import json
import stat
import subprocess
import sys
import tarfile
import tempfile
import unittest
import warnings
import zipfile
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "native_prx_manifest.py"
COMMIT = "0123456789abcdef" * 2 + "01234567"


class NativePrxManifestTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def archive(self, kind, entries):
        path = self.root / ("runtime." + kind)
        if kind == "zip":
            with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                for member, data in entries:
                    archive.writestr(member, data)
        else:
            with tarfile.open(path, "w:gz") as archive:
                for member, data in entries:
                    if isinstance(member, str):
                        member = tarfile.TarInfo(member)
                    member.size = len(data)
                    archive.addfile(member, io.BytesIO(data))
        return path

    def run_tool(self, path, platform="linux", tag="v0.1.5", commit=COMMIT):
        return subprocess.run([sys.executable, str(SCRIPT), str(path), "--platform", platform,
                               "--tag", tag, "--commit", commit], cwd=self.root,
                              capture_output=True, encoding="utf-8", timeout=15)

    def assert_rejected(self, path, diagnostic, **kwargs):
        result = self.run_tool(path, **kwargs)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertIn(diagnostic, result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_zip_and_tar_inventory_same_library_bytes(self):
        for platform, binary, auxiliary in (("linux", b"\x7fELF", "libmimalloc.so"),
                                           ("windows", b"MZ", "mimalloc-redirect.dll")):
            runtime = {"libScePad.prx": binary + b"pad bits", "libc.prx": binary + b"c bits" * 200000,
                       auxiliary: binary + b"allocator bits"}
            entries = [("libs/" + name, data) for name, data in runtime.items()]
            entries.extend([("libs/licenses/mimalloc.txt", b"allocator license"),
                            ("libs/implib/libc.dll.a", b"import library"), ("examples/sample.prx", b"sample")])
            reports = []
            for kind, members in (("zip", entries), ("tar.gz", reversed(entries))):
                with self.subTest(platform=platform, kind=kind):
                    path = self.archive(kind, members)
                    before = (path.read_bytes(), path.stat().st_mtime_ns, set(self.root.iterdir()))
                    result = self.run_tool(path, platform=platform)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(result.stderr, "")
                    report = json.loads(result.stdout)
                    self.assertEqual(report, {"schemaVersion": 1, "tag": "v0.1.5", "commit": COMMIT,
                                              "platform": platform, "files": {
                                                  name: hashlib.sha256(data).hexdigest() for name, data in runtime.items()
                                              }})
                    self.assertEqual(list(report["files"]), sorted(runtime))
                    self.assertEqual(before, (path.read_bytes(), path.stat().st_mtime_ns, set(self.root.iterdir())))
                    reports.append(result.stdout)
            self.assertEqual(reports[0], reports[1])

    def test_rejects_missing_and_empty_prx_members(self):
        for kind in ("zip", "tar.gz"):
            with self.subTest(kind=kind):
                self.assert_rejected(self.archive(kind, []), "contains no libraries")
                self.assert_rejected(self.archive(kind, [("libs/libmimalloc.so", b"data"),
                                                         ("elsewhere/example.prx", b"linkable")]), "contains no libraries")
                self.assert_rejected(self.archive(kind, [("libs/empty.prx", b"")]), "nonempty regular runtime member")

    def test_rejects_duplicate_runtime_paths_in_both_formats(self):
        entries = [("libs/libc.prx", b"one"), ("libs/libc.prx", b"two")]
        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", message="Duplicate name:", category=UserWarning)
            self.assert_rejected(self.archive("zip", entries), "Duplicate runtime member: libs/libc.prx")
        self.assert_rejected(self.archive("tar.gz", entries), "Duplicate runtime member: libs/libc.prx")

    def test_windows_paths_cannot_differ_only_in_letter_case(self):
        path = self.archive("zip", [("libs/libCase.prx", b"one"), ("libs/LIBCASE.prx", b"two")])
        self.assert_rejected(path, "Duplicate runtime member: libs/LIBCASE.prx", platform="windows")
        result = self.run_tool(path)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(set(json.loads(result.stdout)["files"]), {"libCase.prx", "LIBCASE.prx"})

    def test_rejects_link_members(self):
        zip_link = zipfile.ZipInfo("libs/alias.prx")
        zip_link.create_system = 3
        zip_link.external_attr = (stat.S_IFLNK | 0o777) << 16
        path = self.archive("zip", [("libs/libc.prx", b"library"), (zip_link, b"libc.prx")])
        self.assert_rejected(path, "nonempty regular runtime member: libs/alias.prx")
        tar_link = tarfile.TarInfo("libs/alias.prx")
        tar_link.type = tarfile.SYMTYPE
        tar_link.linkname = "libc.prx"
        path = self.archive("tar.gz", [("libs/libc.prx", b"library"), (tar_link, b"")])
        self.assert_rejected(path, "nonempty regular runtime member: libs/alias.prx")

    def test_rejects_invalid_metadata_and_broken_archive(self):
        path = self.archive("zip", [("libs/libc.prx", b"library")])
        self.assert_rejected(path, "Invalid release tag", tag="v0.1.5.")
        self.assert_rejected(path, "full lowercase SHA", commit=COMMIT[:7])
        self.assert_rejected(path, "invalid choice", platform="web")
        path.write_bytes(b"broken ZIP")
        self.assert_rejected(path, "not a zip file")


if __name__ == "__main__":
    unittest.main()
