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

    def test_deeply_nested_api_responses_are_cli_errors_without_site_changes(self):
        deep_json = b"[" * 200000 + b"]" * 200000
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            web = root / "web"
            web.mkdir()
            preserved = web / "keep.txt"
            preserved.write_bytes(b"previous site")
            errors, output = io.StringIO(), io.StringIO()
            with patch.object(build_web, "request", return_value=io.BytesIO(deep_json)), \
                    contextlib.redirect_stderr(errors), contextlib.redirect_stdout(output):
                with self.assertRaises(SystemExit) as raised:
                    build_web.main(["--build", str(root), "--release"])
            self.assertEqual(raised.exception.code, 2)
            self.assertNotIn("Traceback", errors.getvalue())
            self.assertIn("release", errors.getvalue().lower())
            self.assertEqual(output.getvalue(), "")
            self.assertEqual(preserved.read_bytes(), b"previous site")
            self.assertEqual(list(root.iterdir()), [web])
            self.assertEqual(list(web.iterdir()), [preserved])

    def test_deeply_nested_release_metadata_in_libraries_raises_value_error(self):
        deep_json = b"[" * 200000 + b"]" * 200000
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(build_web, "request", return_value=io.BytesIO(deep_json)):
                with self.assertRaisesRegex(ValueError, "release metadata"):
                    build_web.publish_libraries(root)

    def test_deeply_nested_web_release_metadata_is_cli_error_without_site_changes(self):
        deep_json = b"[" * 200000 + b"]" * 200000
        entries = [(name, b"x") for name in build_web.WEB_FILES] + [("web-release.json", deep_json)]
        data = library_zip(entries)
        release = {"tag_name": "v1", "html_url": "https://example.invalid/v1", "assets": [
            {"name": "web-v1.zip", "size": len(data), "browser_download_url": "https://example.invalid/web",
             "digest": "sha256:" + hashlib.sha256(data).hexdigest()},
            *({"name": f"prx-{platform}-v1.zip", "size": 10, "browser_download_url": "https://example.invalid/prx", "digest": None}
              for platform in ("windows", "linux"))]}
        responses = [io.BytesIO(json.dumps(release).encode()), io.BytesIO(data)]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            web = root / "web"
            web.mkdir()
            preserved = web / "keep.txt"
            preserved.write_bytes(b"previous site")
            errors = io.StringIO()
            with patch.object(build_web, "request", side_effect=responses), \
                    contextlib.redirect_stderr(errors), contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaises(SystemExit) as raised:
                    build_web.main(["--build", str(root), "--release"])
            self.assertEqual(raised.exception.code, 2)
            self.assertNotIn("Traceback", errors.getvalue())
            self.assertIn("release metadata", errors.getvalue())
            self.assertEqual(preserved.read_bytes(), b"previous site")
            self.assertEqual(list(root.iterdir()), [web])


class WebReleaseLayoutTests(unittest.TestCase):
    TAG, REVISION = "v1", "a" * 40
    CORE = ("index.html", "app.mjs", "worker.mjs", "native/relinker_wasm.mjs", "native/relinker_wasm.wasm")

    def release(self, names, extra=(), metadata=None):
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for name in (*names, *extra):
                archive.writestr(name, b"content " + name.encode())
            archive.writestr("web-release.json", json.dumps(metadata or {"tag": self.TAG, "revision": self.REVISION}))
        return buffer.getvalue()

    def publish(self, data, expected=None):
        url = "https://example.invalid/web.zip"
        release = {"tag_name": self.TAG, "html_url": "https://example.invalid/v1",
                   "assets": [{"name": f"web-{self.TAG}.zip", "size": len(data), "browser_download_url": url,
                               "digest": "sha256:" + hashlib.sha256(data).hexdigest()}]}
        responses = {"https://api.github.com/repos/boykopovar/AnyPS5/releases/latest": json.dumps(release).encode(), url: data}

        def libraries(staging, _release):
            (staging / "release.json").write_text(json.dumps({"tag": self.TAG}), encoding="utf-8")
        expected_files = expected if expected is not None else set(build_web.WEB_FILES)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "web"
            with patch.object(build_web, "request", side_effect=lambda address: io.BytesIO(responses[address])), \
                    patch.object(build_web, "library_assets"), patch.object(build_web, "publish_libraries", side_effect=libraries), \
                    patch.object(build_web, "release_web_files", return_value=expected_files), \
                    contextlib.redirect_stdout(io.StringIO()):
                try:
                    build_web.publish_release(output)
                finally:
                    self.published = sorted(str(file.relative_to(output)) for file in output.rglob("*") if file.is_file()) if output.exists() else []

    def test_current_release_with_every_web_file_is_published(self):
        self.publish(self.release(build_web.WEB_FILES))
        self.assertIn("failure-report.mjs", self.published)

    def test_release_lacking_a_newer_web_file_is_published_as_shipped(self):
        older = [name for name in build_web.WEB_FILES if name != "failure-report.mjs"]
        self.publish(self.release(older), expected=set(older))
        self.assertNotIn("failure-report.mjs", self.published)
        self.assertIn("app.mjs", self.published)
        self.assertIn("release.json", self.published)

    def test_release_with_an_unknown_file_is_rejected(self):
        for extra in ("unlisted.mjs", "native/extra.wasm", "../escape.mjs", "app.mjs.bak"):
            with self.subTest(extra=extra):
                with self.assertRaisesRegex(ValueError, "Invalid web release archive layout"):
                    self.publish(self.release(build_web.WEB_FILES, extra=(extra,)))
                self.assertEqual(self.published, [])

    def test_release_missing_a_core_file_is_rejected(self):
        for missing in self.CORE:
            with self.subTest(missing=missing):
                with self.assertRaisesRegex(ValueError, "Invalid web release archive layout"):
                    self.publish(self.release([name for name in build_web.WEB_FILES if name != missing]))
                self.assertEqual(self.published, [])

    def test_release_with_a_duplicate_entry_is_rejected(self):
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            data = self.release(build_web.WEB_FILES, extra=("app.mjs",))
        with self.assertRaisesRegex(ValueError, "Invalid web release archive layout"):
            self.publish(data)

    def test_release_with_an_empty_file_or_mismatched_metadata_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "does not match the selected release"):
            self.publish(self.release(build_web.WEB_FILES, metadata={"tag": "v0", "revision": self.REVISION}))
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, "w") as archive:
            for name in build_web.WEB_FILES:
                archive.writestr(name, b"" if name == "app.mjs" else b"x")
            archive.writestr("web-release.json", json.dumps({"tag": self.TAG, "revision": self.REVISION}))
        with self.assertRaisesRegex(ValueError, "Invalid expanded web release archive size"):
            self.publish(buffer.getvalue())


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


class WebReleaseValidationTests(unittest.TestCase):
    HISTORICAL_FILES = (
        "index.html", "app.css", "app.mjs", "archive.mjs", "batch.mjs",
        "output-directory.mjs", "worker.mjs", "native/relinker_wasm.mjs", "native/relinker_wasm.wasm",
    )
    HISTORICAL_REVISION = "3fd1cc393b8512c2e989860053f30dee5a8cd1aa"

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / "build"
        self.output = self.build / "web"
        self.output.mkdir(parents=True)
        self.prx = library_zip([("libs/libc.prx", b"libc")])

    def publish(self, web_archive, tag="v0.1.3"):
        responses = {
            f"https://example.invalid/web-{tag}.zip": web_archive,
            f"https://example.invalid/windows-{tag}.zip": self.prx,
            f"https://example.invalid/linux-{tag}.zip": self.prx,
        }
        release = {
            "tag_name": tag,
            "html_url": f"https://example.invalid/{tag}",
            "assets": [
                {"name": f"web-{tag}.zip", "size": len(web_archive),
                 "digest": "sha256:" + hashlib.sha256(web_archive).hexdigest(),
                 "browser_download_url": f"https://example.invalid/web-{tag}.zip"},
                {"name": f"prx-windows-{tag}.zip", "size": len(self.prx),
                 "digest": "sha256:" + hashlib.sha256(self.prx).hexdigest(),
                 "browser_download_url": f"https://example.invalid/windows-{tag}.zip"},
                {"name": f"prx-linux-{tag}.zip", "size": len(self.prx),
                 "digest": "sha256:" + hashlib.sha256(self.prx).hexdigest(),
                 "browser_download_url": f"https://example.invalid/linux-{tag}.zip"},
            ],
        }
        responses["https://api.github.com/repos/boykopovar/AnyPS5/releases/latest"] = json.dumps(release).encode()
        with patch.object(build_web, "request", side_effect=lambda url: io.BytesIO(responses[url])), \
                contextlib.redirect_stdout(io.StringIO()):
            build_web.publish_release(self.output)

    def test_historical_release_missing_later_files_is_accepted(self):
        entries = [(name, f"content of {name}".encode()) for name in self.HISTORICAL_FILES]
        entries.append(("web-release.json", json.dumps({"tag": "v0.1.3", "revision": self.HISTORICAL_REVISION}).encode()))
        archive = library_zip(entries)
        with patch.object(build_web, "release_web_files", return_value=set(self.HISTORICAL_FILES)):
            self.publish(archive, tag="v0.1.3")
        self.assertTrue((self.output / "index.html").is_file())
        self.assertFalse((self.output / "failure-report.mjs").exists())
        manifest = json.loads((self.output / "release.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["revision"], self.HISTORICAL_REVISION)

    def test_current_release_with_expected_files_is_accepted(self):
        revision = subprocess.run(["git", "-C", str(build_web.ROOT), "rev-parse", "HEAD"],
                                  capture_output=True, text=True, check=True).stdout.strip()
        entries = [(name, f"content of {name}".encode()) for name in build_web.WEB_FILES]
        entries.append(("web-release.json", json.dumps({"tag": "v1.0.0", "revision": revision}).encode()))
        archive = library_zip(entries)
        self.publish(archive, tag="v1.0.0")
        for name in build_web.WEB_FILES:
            self.assertTrue((self.output / name).is_file())

    def test_invalid_archive_entries_are_rejected(self):
        valid = [(name, b"valid") for name in self.HISTORICAL_FILES]
        metadata = json.dumps({"tag": "v0.1.3", "revision": self.HISTORICAL_REVISION}).encode()
        unexpected = library_zip([*valid, ("unexpected.txt", b"extra"), ("web-release.json", metadata)])
        missing = library_zip([*valid[1:], ("web-release.json", metadata)])
        duplicate = library_zip([*valid, valid[0], ("web-release.json", metadata)])
        for name, archive in (("unexpected", unexpected), ("missing", missing), ("duplicate", duplicate)):
            with self.subTest(case=name):
                with self.assertRaisesRegex(ValueError, "Invalid web release archive layout"):
                    self.publish(archive, tag="v0.1.3")

    def test_unavailable_historical_revision_fails_cleanly_without_traceback_or_site_mutation(self):
        marker = self.output / "previous.txt"
        marker.write_bytes(b"preserved site")
        entries = [(name, b"content") for name in self.HISTORICAL_FILES]
        entries.append(("web-release.json", json.dumps({"tag": "v0.1.3", "revision": "0" * 40}).encode()))
        archive = library_zip(entries)
        responses = {
            "https://example.invalid/web-v0.1.3.zip": archive,
            "https://example.invalid/windows-v0.1.3.zip": self.prx,
            "https://example.invalid/linux-v0.1.3.zip": self.prx,
        }
        release = {
            "tag_name": "v0.1.3",
            "html_url": "https://example.invalid/v0.1.3",
            "assets": [
                {"name": "web-v0.1.3.zip", "size": len(archive),
                 "digest": "sha256:" + hashlib.sha256(archive).hexdigest(),
                 "browser_download_url": "https://example.invalid/web-v0.1.3.zip"},
                {"name": "prx-windows-v0.1.3.zip", "size": len(self.prx),
                 "digest": "sha256:" + hashlib.sha256(self.prx).hexdigest(),
                 "browser_download_url": "https://example.invalid/windows-v0.1.3.zip"},
                {"name": "prx-linux-v0.1.3.zip", "size": len(self.prx),
                 "digest": "sha256:" + hashlib.sha256(self.prx).hexdigest(),
                 "browser_download_url": "https://example.invalid/linux-v0.1.3.zip"},
            ],
        }
        responses["https://api.github.com/repos/boykopovar/AnyPS5/releases/latest"] = json.dumps(release).encode()
        stdout, stderr = io.StringIO(), io.StringIO()
        with patch.object(build_web, "request", side_effect=lambda url: io.BytesIO(responses[url])), \
                contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr), \
                self.assertRaises(SystemExit) as exit_error:
            build_web.main(["--build", str(self.build), "--release"])
        self.assertEqual(exit_error.exception.code, 2)
        self.assertIn("error: Cannot resolve release revision: 0000000000000000000000000000000000000000", stderr.getvalue())
        self.assertNotIn("Traceback", stderr.getvalue())
        self.assertEqual(marker.read_bytes(), b"preserved site")

    def test_invalid_web_files_definition_fails_cleanly_without_traceback_or_site_mutation(self):
        marker = self.output / "previous.txt"
        marker.write_bytes(b"preserved site")
        entries = [(name, b"content") for name in self.HISTORICAL_FILES]
        entries.append(("web-release.json", json.dumps({"tag": "v0.1.3", "revision": "1" * 40}).encode()))
        archive = library_zip(entries)
        responses = {
            "https://example.invalid/web-v0.1.3.zip": archive,
            "https://example.invalid/windows-v0.1.3.zip": self.prx,
            "https://example.invalid/linux-v0.1.3.zip": self.prx,
        }
        release = {
            "tag_name": "v0.1.3",
            "html_url": "https://example.invalid/v0.1.3",
            "assets": [
                {"name": "web-v0.1.3.zip", "size": len(archive),
                 "digest": "sha256:" + hashlib.sha256(archive).hexdigest(),
                 "browser_download_url": "https://example.invalid/web-v0.1.3.zip"},
                {"name": "prx-windows-v0.1.3.zip", "size": len(self.prx),
                 "digest": "sha256:" + hashlib.sha256(self.prx).hexdigest(),
                 "browser_download_url": "https://example.invalid/windows-v0.1.3.zip"},
                {"name": "prx-linux-v0.1.3.zip", "size": len(self.prx),
                 "digest": "sha256:" + hashlib.sha256(self.prx).hexdigest(),
                 "browser_download_url": "https://example.invalid/linux-v0.1.3.zip"},
            ],
        }
        responses["https://api.github.com/repos/boykopovar/AnyPS5/releases/latest"] = json.dumps(release).encode()
        stdout, stderr = io.StringIO(), io.StringIO()
        with patch.object(subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "WEB_FILES = None", "")), \
                patch.object(build_web, "request", side_effect=lambda url: io.BytesIO(responses[url])), \
                contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr), \
                self.assertRaises(SystemExit) as exit_error:
            build_web.main(["--build", str(self.build), "--release"])
        self.assertEqual(exit_error.exception.code, 2)
        self.assertIn("error: Invalid web release definition in revision: 1111111111111111111111111111111111111111", stderr.getvalue())
        self.assertNotIn("Traceback", stderr.getvalue())
        self.assertEqual(marker.read_bytes(), b"preserved site")

    def test_invalid_metadata_fails_cleanly(self):
        valid = [(name, b"valid") for name in self.HISTORICAL_FILES]
        bad_json = library_zip([*valid, ("web-release.json", b"{invalid json")])
        bad_tag = library_zip([*valid, ("web-release.json", json.dumps({"tag": "v9.9.9", "revision": self.HISTORICAL_REVISION}).encode())])
        for name, archive, err in (("bad_json", bad_json, "Invalid web release metadata"),
                                   ("bad_tag", bad_tag, "Web converter does not match the selected release")):
            with self.subTest(case=name):
                with self.assertRaisesRegex(ValueError, err):
                    self.publish(archive, tag="v0.1.3")

    def test_historical_web_files_parser_rejects_invalid_or_duplicate_entries(self):
        definitions = (
            'WEB_FILES = ("index.html", "index.html")',
            'WEB_FILES = ("../escape",)',
            'WEB_FILES = ("",)',
            'WEB_FILES = ("/absolute",)',
            r'WEB_FILES = ("nested\\backslash",)',
            'WEB_FILES = ("nested/./current",)',
            'WEB_FILES = [1, 2]',
            'WEB_FILES = ([1],)',
            'WEB_FILES = None',
        )
        for code in definitions:
            with self.subTest(code=code):
                with patch.object(subprocess, "run", return_value=subprocess.CompletedProcess([], 0, code, "")):
                    with self.assertRaisesRegex(ValueError, "Invalid web release definition in revision"):
                        build_web.release_web_files("0" * 40)

    def test_historical_web_files_parser_rejects_missing_revision(self):
        with patch.object(subprocess, "run", return_value=subprocess.CompletedProcess([], 128, "", "fatal: bad object")):
            with self.assertRaisesRegex(ValueError, "Cannot resolve release revision: 0000000000000000000000000000000000000000"):
                build_web.release_web_files("0" * 40)


if __name__ == "__main__":
    unittest.main()
