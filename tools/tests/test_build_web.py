import os
import contextlib
import hashlib
import io
import json
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import warnings
import zipfile
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_web


def library_zip(entries):
    buffer = io.BytesIO()
    with warnings.catch_warnings(), zipfile.ZipFile(buffer, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        warnings.simplefilter("ignore", UserWarning)
        for name, data in entries:
            archive.writestr(name, data)
    return buffer.getvalue()


class LibraryPublicationTests(unittest.TestCase):
    def test_unicode_path_rewrite_is_rejected_before_decompression(self):
        original = "libs/../libc.prx"
        rewritten = "libs/libc.prx"
        entry = zipfile.ZipInfo(original)
        unicode_path = b"\x01" + struct.pack("<I", zlib.crc32(original.encode())) + rewritten.encode()
        entry.extra = struct.pack("<HH", 0x7075, len(unicode_path)) + unicode_path
        invalid = library_zip([(entry, b"content")])
        with zipfile.ZipFile(io.BytesIO(invalid)) as archive:
            decoded = archive.infolist()[0]
            self.assertEqual(decoded.orig_filename, original)
            if decoded.filename == original:
                self.skipTest("zipfile does not interpret Unicode Path extra fields")
            self.assertEqual(decoded.filename, rewritten)
        valid = library_zip([("libs/libc.prx", b"valid")])
        with patch.object(zipfile.ZipFile, "testzip", side_effect=AssertionError("archive decompressed")):
            with self.assertRaisesRegex(ValueError, "Invalid library archive layout"):
                self.publish(invalid, valid)
        self.assertEqual(self.manifest.read_bytes(), self.previous)
        self.assertEqual(list(self.libraries.iterdir()), [self.existing])

    def test_original_archive_paths_are_rejected_before_decompression(self):
        valid = library_zip([("libs/libc.prx", b"valid")])
        for name in ("libs/libc.prx\0ignored", "libs/sub\0dir/libc.prx",
                     "libs\\libc.prx", "libs/sub\\libc.prx"):
            with self.subTest(name=name):
                placeholder = name.replace("\0", "!").replace("\\", "!")
                invalid = library_zip([(placeholder, b"content")])
                invalid = invalid.replace(placeholder.encode(), name.encode())
                with zipfile.ZipFile(io.BytesIO(invalid)) as archive:
                    self.assertEqual(archive.infolist()[0].orig_filename, name)
                with patch.object(zipfile.ZipFile, "testzip", side_effect=AssertionError("archive decompressed")):
                    with self.assertRaisesRegex(ValueError, "Invalid library archive layout"):
                        self.publish(invalid, valid)
                self.assertEqual(self.manifest.read_bytes(), self.previous)
                self.assertEqual(list(self.libraries.iterdir()), [self.existing])

    def test_rewritten_path_in_second_platform_preserves_previous_manifest(self):
        valid = library_zip([("libs/libc.prx", b"valid")])
        invalid = library_zip([("libs/libc.prx!ignored", b"content")])
        invalid = invalid.replace(b"libs/libc.prx!ignored", b"libs/libc.prx\0ignored")
        with self.assertRaisesRegex(ValueError, "Invalid library archive layout"):
            self.publish(valid, invalid)
        self.assertEqual(self.manifest.read_bytes(), self.previous)
        self.assertEqual(self.existing.read_bytes(), b"previous archive")
        self.assertFalse(any(file.suffix == ".download" for file in self.libraries.iterdir()))

    def test_unicode_archive_paths_are_published_unchanged(self):
        payload = library_zip([("libs/\u65e5\u672c\u8a9e/\u00e9.prx", b"content"),
                               ("libs/licenses/Third party.txt", b"license")])
        self.publish(payload, payload)
        manifest = json.loads(self.manifest.read_text(encoding="utf-8"))
        for asset in manifest["platforms"].values():
            self.assertEqual((self.output / asset["path"]).read_bytes(), payload)

    def test_expanded_limit_is_checked_before_decompression(self):
        payload = library_zip([("libs/libc.prx", b"a" * 4097)])
        with patch.object(build_web, "MAX_EXPANDED_BYTES", 4096, create=True), \
                patch.object(zipfile.ZipFile, "testzip", side_effect=AssertionError("archive decompressed")):
            with self.assertRaisesRegex(ValueError, "expanded size"):
                self.publish(payload, payload)
        self.assertEqual(self.manifest.read_bytes(), self.previous)
        self.assertEqual(list(self.libraries.iterdir()), [self.existing])


    def test_expanded_limit_boundary_can_be_published(self):
        payload = library_zip([("libs/libc.prx", b"a" * 4096)])
        with patch.object(build_web, "MAX_EXPANDED_BYTES", 4096, create=True):
            self.publish(payload, payload)
        report = json.loads(self.manifest.read_text(encoding="utf-8"))
        self.assertTrue(all(asset["expandedBytes"] == 4096 for asset in report["platforms"].values()))

    def test_file_ancestors_are_rejected_before_publication(self):
        valid = library_zip([("libs/libc.prx", b"valid")])
        for entries in (("libs/nested", "libs/nested/libc.prx"),
                        ("libs/NESTED/libc.prx", "libs/nested")):
            with self.subTest(entries=entries):
                invalid = library_zip([(name, b"content") for name in entries])
                with self.assertRaisesRegex(ValueError, "file is also used as a directory"):
                    self.publish(invalid, valid)
                self.assertEqual(self.manifest.read_bytes(), self.previous)
                self.assertEqual(list(self.libraries.iterdir()), [self.existing])


    def test_explicit_and_implicit_archive_directories_are_valid(self):
        for entries in ([('libs/sub/libc.prx', b'content')],
                        [('libs/sub/', b''), ('libs/sub/libc.prx', b'content')]):
            with self.subTest(entries=entries):
                payload = library_zip(entries)
                self.publish(payload, payload)
                self.assertEqual(json.loads(self.manifest.read_text(encoding="utf-8"))["tag"], "v1")


    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.output = Path(temporary.name)
        self.libraries = self.output / "libraries"
        self.libraries.mkdir()
        self.manifest = self.output / "release.json"
        self.previous = b'{"tag":"previous complete release"}\n'
        self.manifest.write_bytes(self.previous)
        self.existing = self.libraries / "previous.zip"
        self.existing.write_bytes(b"previous archive")

    def publish(self, windows, linux):
        assets = []
        responses = {}
        for platform, data in (("windows", windows), ("linux", linux)):
            url = f"https://example.invalid/{platform}.zip"
            assets.append({"name": f"prx-{platform}-v1.zip", "size": len(data),
                           "digest": "sha256:" + hashlib.sha256(data).hexdigest(), "browser_download_url": url})
            responses[url] = data
        release = {"tag_name": "v1", "html_url": "https://example.invalid/v1", "assets": assets}
        responses["https://api.github.com/repos/boykopovar/AnyPS5/releases/latest"] = json.dumps(release).encode()
        with patch.object(build_web, "request", side_effect=lambda url: io.BytesIO(responses[url])), \
                contextlib.redirect_stdout(io.StringIO()):
            build_web.publish_libraries(self.output)

    def test_colliding_paths_do_not_publish_a_release(self):
        valid = library_zip([("libs/libc.prx", b"valid archive")])
        for first, second in (("libs/libc.prx", "libs/libc.prx"),
                              ("libs/libc.prx", "libs/LIBC.prx"),
                              ("libs/extra/", "libs/EXTRA")):
            with self.subTest(first=first, second=second):
                invalid = library_zip([(first, b"" if first.endswith("/") else b"first"), (second, b"second")])
                with self.assertRaisesRegex(ValueError, "Duplicate library archive path"):
                    self.publish(invalid, valid)
                self.assertEqual(self.manifest.read_bytes(), self.previous)
                self.assertEqual(self.existing.read_bytes(), b"previous archive")
                self.assertEqual(list(self.libraries.iterdir()), [self.existing])

    def test_collision_in_second_platform_preserves_previous_manifest(self):
        valid = library_zip([("libs/libc.prx", b"valid archive")])
        invalid = library_zip([("libs/libc.prx", b"first"), ("libs/LIBC.prx", b"second")])
        with self.assertRaisesRegex(ValueError, "Duplicate library archive path"):
            self.publish(valid, invalid)
        self.assertEqual(self.manifest.read_bytes(), self.previous)
        self.assertEqual(self.existing.read_bytes(), b"previous archive")
        self.assertFalse(any(file.suffix == ".download" for file in self.libraries.iterdir()))

    def test_distinct_full_paths_publish_both_platforms(self):
        payload = library_zip([("libs/", b""), ("libs/libc.prx", b"first"), ("libs/extra/libc.prx", b"second")])
        self.publish(payload, payload)
        manifest = json.loads(self.manifest.read_text(encoding="utf-8"))
        self.assertEqual(manifest["tag"], "v1")
        self.assertEqual(set(manifest["platforms"]), {"windows", "linux"})
        for platform, asset in manifest["platforms"].items():
            digest = hashlib.sha256(payload).hexdigest()
            self.assertEqual(asset, {"path": f"libraries/{platform}-{digest}.zip", "size": len(payload),
                                     "expandedBytes": 11, "sha256": digest})
            self.assertEqual((self.output / asset["path"]).read_bytes(), payload)
        self.assertEqual(self.existing.read_bytes(), b"previous archive")
        self.assertFalse(any(file.suffix == ".download" for file in self.libraries.iterdir()))


    def test_browser_incompatible_compression_and_special_files_preserve_old_release(self):
        valid = library_zip([("libs/libc.prx", b"valid")])
        for compression in (zipfile.ZIP_BZIP2, zipfile.ZIP_LZMA):
            data = io.BytesIO()
            with zipfile.ZipFile(data, "w", compression=compression) as archive:
                archive.writestr("libs/libc.prx", b"content")
            with self.subTest(compression=compression), self.assertRaisesRegex(ValueError, "compression"):
                self.publish(data.getvalue(), valid)
            self.assertEqual(self.manifest.read_bytes(), self.previous)
            self.assertEqual(list(self.libraries.iterdir()), [self.existing])
        for mode in (0o010644, 0o020644, 0o060644, 0o120777, 0o140644):
            entry = zipfile.ZipInfo("libs/special")
            entry.create_system = 3
            entry.external_attr = mode << 16
            with self.subTest(mode=mode), self.assertRaisesRegex(ValueError, "special files"):
                self.publish(library_zip([(entry, b"content")]), valid)
            self.assertEqual(self.manifest.read_bytes(), self.previous)
        for compression in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED):
            data = io.BytesIO()
            with zipfile.ZipFile(data, "w", compression=compression) as archive:
                archive.writestr("libs/libc.prx", b"valid")
            self.publish(data.getvalue(), data.getvalue())
            self.assertEqual(json.loads(self.manifest.read_text(encoding="utf-8"))["tag"], "v1")


class BuildWebCliTests(unittest.TestCase):
    def test_missing_relinker_wasm_reports_error_without_traceback(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            result = subprocess.run([sys.executable, str(Path(build_web.__file__)), "--build", str(build)],
                                    capture_output=True, text=True,
                                    env={**os.environ, "PYTHONPATH": str(Path(__file__).resolve().parents[1])})
            self.assertEqual(result.returncode, 2)
            self.assertIn("Build relinker_wasm first: missing", result.stderr)
            self.assertNotIn("Traceback", result.stderr)

    def test_main_with_explicit_argv(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            native = build / "web" / "native"
            native.mkdir(parents=True)
            (native / "relinker_wasm.mjs").write_bytes(b"export default {};")
            (native / "relinker_wasm.wasm").write_bytes(b"\0asm\x01\0\0\0")
            with contextlib.redirect_stdout(io.StringIO()):
                code = build_web.main(["--build", str(build)])
            self.assertEqual(code, 0)

    def test_main_reconfigures_streams(self):
        class Stream:
            def __init__(self):
                self.encoding = None
                self.errors = None

            def reconfigure(self, encoding=None, errors=None):
                self.encoding = encoding
                self.errors = errors

            def write(self, text):
                pass

            def flush(self):
                pass

        out, err = Stream(), Stream()
        with patch.object(build_web.sys, "stdout", out), patch.object(build_web.sys, "stderr", err):
            try:
                build_web.main([])
            except SystemExit:
                pass
        self.assertEqual(out.encoding, "utf-8")
        self.assertEqual(out.errors, "backslashreplace")
        self.assertEqual(err.encoding, "utf-8")
        self.assertEqual(err.errors, "backslashreplace")


class ReleaseMetadataTests(unittest.TestCase):
    def test_malformed_api_responses_are_cli_errors_without_site_changes(self):
        documents = ({}, [], None, {"tag_name": 1}, {"tag_name": "v1", "html_url": "url", "assets": None},
                     {"tag_name": "v1", "html_url": "url", "assets": [None]},
                     {"tag_name": "v1", "html_url": "url", "assets": [{"name": 1}]})
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            web = root / "web"
            web.mkdir()
            preserved = web / "keep.txt"
            preserved.write_bytes(b"previous site")
            for document in documents:
                with self.subTest(document=document):
                    errors, output = io.StringIO(), io.StringIO()
                    with patch.object(build_web, "request", return_value=io.BytesIO(json.dumps(document).encode())), contextlib.redirect_stderr(errors), contextlib.redirect_stdout(output):
                        with self.assertRaises(SystemExit) as raised:
                            build_web.main(["--build", str(root), "--release"])
                    self.assertEqual(raised.exception.code, 2)
                    self.assertNotIn("Traceback", errors.getvalue())
                    self.assertIn("release", errors.getvalue().lower())
                    self.assertEqual(output.getvalue(), "")
                    self.assertEqual(preserved.read_bytes(), b"previous site")
                    self.assertEqual(list(root.iterdir()), [web])
                    self.assertEqual(list(web.iterdir()), [preserved])

    def test_selected_asset_metadata_is_checked_before_any_download(self):
        def asset(platform):
            return {"name": f"prx-{platform}-v1.zip", "size": 10, "browser_download_url": "https://example.invalid/asset", "digest": None}
        changes = [("size", x) for x in (True, 0, -1, 1.5, "10", 512*1024*1024+1)]
        changes += [("browser_download_url", x) for x in (None, "", 1)]
        changes += [("digest", x) for x in (False, 1, [])]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            existing = root / "release.json"
            existing.write_bytes(b"previous manifest")
            for field, value in changes:
                with self.subTest(field=field, value=value):
                    windows, linux = asset("windows"), asset("linux")
                    linux[field] = value
                    release = {"tag_name": "v1", "html_url": "https://example.invalid/v1", "assets": [windows, linux]}
                    with patch.object(build_web, "request", side_effect=AssertionError("download occurred before preflight")):
                        with self.assertRaisesRegex(ValueError, "release asset"):
                            build_web.publish_libraries(root, release)
                    self.assertEqual(existing.read_bytes(), b"previous manifest")
                    self.assertEqual(list(root.iterdir()), [existing])



class LocalNativeArtifactTests(unittest.TestCase):
    def test_empty_or_missing_native_input_does_not_copy_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            native = build / "web/native"
            native.mkdir(parents=True)
            marker = build / "web/app.mjs"
            marker.write_bytes(b"preserve old site")
            for name in ("relinker_wasm.mjs", "relinker_wasm.wasm"):
                for missing in (False, True):
                    with self.subTest(name=name, missing=missing):
                        for artifact in ("relinker_wasm.mjs", "relinker_wasm.wasm"):
                            (native / artifact).write_bytes(b"nonempty fixture")
                        if missing:
                            (native / name).unlink()
                        else:
                            (native / name).write_bytes(b"")
                        stdout, stderr = io.StringIO(), io.StringIO()
                        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr), self.assertRaises(SystemExit) as raised:
                            build_web.main(["--build", str(build)])
                        self.assertEqual(raised.exception.code, 2)
                        self.assertIn(("missing " if missing else "empty ") + name, stderr.getvalue())
                        self.assertEqual(stdout.getvalue(), "")
                        self.assertEqual(marker.read_bytes(), b"preserve old site")
                        self.assertFalse((build / "web/.nojekyll").exists())
            for artifact in ("relinker_wasm.mjs", "relinker_wasm.wasm"):
                (native / artifact).write_bytes(b"nonempty fixture")
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(build_web.main(["--build", str(build)]), 0)
            self.assertEqual(marker.read_bytes(), (build_web.ROOT / "demo/web/app.mjs").read_bytes())
            self.assertTrue((build / "web/.nojekyll").exists())


if __name__ == "__main__":
    unittest.main()
