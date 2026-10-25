import contextlib
import hashlib
import io
import json
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_web
import package_release
import test_web

FILES = ("index.html", "app.css", "app.mjs", "archive.mjs", "batch.mjs", "output-directory.mjs", "worker.mjs",
         "native/relinker_wasm.mjs", "native/relinker_wasm.wasm")
REVISION = "a" * 40
LATEST = "https://api.github.com/repos/boykopovar/AnyPS5/releases/latest"


def zipped(entries):
    data = io.BytesIO()
    with zipfile.ZipFile(data, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, content in entries:
            archive.writestr(name, content)
    return data.getvalue()


class WebReleaseTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.build = self.root / "build"
        self.output = self.build / "web"
        self.output.mkdir(parents=True)
        self.contents = {name: ("release " + name).encode() for name in FILES}
        self.metadata = {"tag": "v1", "revision": REVISION}
        self.entries = [*self.contents.items(), ("web-release.json", json.dumps(self.metadata).encode())]
        self.responses = {}
        self.release = {"tag_name": "v1", "html_url": "https://github.com/boykopovar/AnyPS5/releases/tag/v1", "assets": []}
        self.set_asset("web-v1.zip", zipped(self.entries))
        for platform in ("windows", "linux"):
            self.set_asset(f"prx-{platform}-v1.zip", zipped([("libs/libkernel.prx", platform.encode())]))
        self.requests = []

    def set_asset(self, name, content):
        url = "https://example.test/" + name
        self.responses[url] = content
        asset = {"name": name, "browser_download_url": url, "size": len(content),
                 "digest": "sha256:" + hashlib.sha256(content).hexdigest()}
        self.release["assets"] = [item for item in self.release["assets"] if item["name"] != name] + [asset]
        return asset

    def request(self, url):
        self.requests.append(url)
        if url == LATEST:
            if self.requests.count(LATEST) > 1:
                raise AssertionError("Release selection changed during publication")
            return io.BytesIO(json.dumps(self.release).encode())
        return io.BytesIO(self.responses[url])

    def publish(self):
        with patch.object(build_web, "request", self.request):
            build_web.publish_release(self.output)

    def snapshot(self):
        return {path.relative_to(self.output).as_posix(): path.read_bytes()
                for path in self.output.rglob("*") if path.is_file()}

    def seed_previous(self):
        for name, content in {"index.html": b"old page", "native/relinker_wasm.wasm": b"newer main wasm",
                              "release.json": b"old manifest", "libraries/old.zip": b"old libraries"}.items():
            path = self.output / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)

    def assert_rejected(self, message):
        self.seed_previous()
        previous = self.snapshot()
        with self.assertRaisesRegex(ValueError, message):
            self.publish()
        self.assertEqual(self.snapshot(), previous)
        self.assertEqual(list(self.build.glob(".web-release-*")), [])

    def test_release_replaces_main_converter_with_matching_frontend_and_libraries(self):
        self.seed_previous()
        self.publish()
        for name, content in self.contents.items():
            self.assertEqual((self.output / name).read_bytes(), content)
        self.assertEqual(json.loads((self.output / "web-release.json").read_text()), self.metadata)
        manifest = json.loads((self.output / "release.json").read_text())
        self.assertEqual((manifest["tag"], manifest["revision"], manifest["url"]),
                         ("v1", REVISION, self.release["html_url"]))
        for platform in ("windows", "linux"):
            asset = manifest["platforms"][platform]
            data = (self.output / asset["path"]).read_bytes()
            self.assertEqual(len(data), asset["size"])
            self.assertEqual(hashlib.sha256(data).hexdigest(), asset["sha256"])
            with zipfile.ZipFile(io.BytesIO(data)) as archive:
                self.assertEqual(archive.read("libs/libkernel.prx"), platform.encode())
        self.assertTrue((self.output / ".nojekyll").is_file())
        self.assertEqual(self.requests.count(LATEST), 1)

    def test_rewritten_library_paths_preserve_the_previous_site(self):
        invalid = zipped([("libs/libkernel.prx!ignored", b"content")])
        invalid = invalid.replace(b"libs/libkernel.prx!ignored", b"libs/libkernel.prx\0ignored")
        for platform in ("windows", "linux"):
            with self.subTest(platform=platform):
                self.requests.clear()
                name = f"prx-{platform}-v1.zip"
                original = self.responses["https://example.test/" + name]
                self.set_asset(name, invalid)
                try:
                    self.assert_rejected("Invalid library archive layout")
                finally:
                    self.set_asset(name, original)

    def test_libraries_flag_also_replaces_an_incompatible_local_converter(self):
        self.seed_previous()
        source = self.root / "source/demo/web"
        source.mkdir(parents=True)
        (source / "index.html").write_bytes(b"main frontend")
        (self.output / "native/relinker_wasm.mjs").write_bytes(b"main converter")
        with patch.object(build_web, "ROOT", self.root / "source"), \
                patch.object(build_web, "request", self.request), \
                patch.object(sys, "argv", ["build_web.py", "--build", str(self.build), "--libraries"]):
            build_web.main()
        self.assertEqual((self.output / "native/relinker_wasm.wasm").read_bytes(), self.contents["native/relinker_wasm.wasm"])
        self.assertEqual((self.output / "index.html").read_bytes(), self.contents["index.html"])

    def test_publication_replaces_obsolete_files_and_directory_conflicts(self):
        self.seed_previous()
        conflict = self.output / "native/relinker_wasm.mjs"
        conflict.mkdir()
        (conflict / "obsolete.txt").write_bytes(b"previous layout")
        self.publish()
        expected = {*FILES, "web-release.json", "release.json", ".nojekyll"}
        manifest = json.loads((self.output / "release.json").read_text())
        expected.update(asset["path"] for asset in manifest["platforms"].values())
        self.assertEqual(set(self.snapshot()), expected)
        for name, content in self.contents.items():
            self.assertEqual((self.output / name).read_bytes(), content)

    def test_failed_publication_restores_the_previous_site(self):
        self.seed_previous()
        previous = self.snapshot()
        rename = Path.rename

        def failed_rename(path, destination):
            if path != self.output and Path(destination) == self.output and path.name == "site":
                raise PermissionError("publication blocked")
            return rename(path, destination)

        with patch.object(Path, "rename", failed_rename):
            with self.assertRaisesRegex(PermissionError, "publication blocked"):
                self.publish()
        self.assertEqual(self.snapshot(), previous)
        self.assertEqual(list(self.build.glob(".web-release-*")), [])

    def test_failed_first_publication_leaves_no_output(self):
        self.output.rmdir()
        rename = Path.rename

        def failed_rename(path, destination):
            if Path(destination) == self.output:
                raise PermissionError("publication blocked")
            return rename(path, destination)

        with patch.object(Path, "rename", failed_rename):
            with self.assertRaisesRegex(PermissionError, "publication blocked"):
                self.publish()
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.build.glob(".web-release-*")), [])

    def test_output_file_is_rejected_without_replacing_it(self):
        self.output.rmdir()
        self.output.write_bytes(b"unrelated file")
        with self.assertRaisesRegex(ValueError, "Release output is not a directory"):
            self.publish()
        self.assertEqual(self.output.read_bytes(), b"unrelated file")
        self.assertEqual(list(self.build.glob(".web-release-*")), [])

    def test_failed_backup_keeps_the_previous_site(self):
        self.seed_previous()
        previous = self.snapshot()
        rename = Path.rename

        def failed_rename(path, destination):
            if path == self.output:
                raise PermissionError("previous site is in use")
            return rename(path, destination)

        with patch.object(Path, "rename", failed_rename):
            with self.assertRaisesRegex(PermissionError, "previous site is in use"):
                self.publish()
        self.assertEqual(self.snapshot(), previous)
        self.assertEqual(list(self.build.glob(".web-release-*")), [])

    def test_failed_restoration_retains_the_backup_and_reports_its_path(self):
        self.seed_previous()
        previous = self.snapshot()
        rename = Path.rename

        def failed_rename(path, destination):
            if path != self.output and Path(destination) == self.output:
                raise PermissionError("destination is unavailable")
            return rename(path, destination)

        with patch.object(Path, "rename", failed_rename):
            with self.assertRaisesRegex(RuntimeError, "Previous site is preserved at") as error:
                self.publish()
        temporary, = self.build.glob(".web-release-*")
        backup = temporary / "previous"
        self.assertIn(str(backup), str(error.exception))
        self.assertEqual({path.relative_to(backup).as_posix(): path.read_bytes()
                          for path in backup.rglob("*") if path.is_file()}, previous)
        self.assertFalse(self.output.exists())

    def test_release_mode_does_not_require_a_local_wasm_build(self):
        with patch.object(build_web, "request", self.request), \
                patch.object(sys, "argv", ["build_web.py", "--build", str(self.build), "--release"]):
            build_web.main()
        self.assertEqual((self.output / "native/relinker_wasm.mjs").read_bytes(), self.contents["native/relinker_wasm.mjs"])

    def test_local_build_still_copies_sources_without_network(self):
        source = self.root / "source/demo/web"
        source.mkdir(parents=True)
        (source / "index.html").write_bytes(b"local frontend")
        (self.output / "native").mkdir()
        for name in ("relinker_wasm.mjs", "relinker_wasm.wasm"):
            (self.output / "native" / name).write_bytes(b"local wasm")
        with patch.object(build_web, "ROOT", self.root / "source"), \
                patch.object(build_web, "request", side_effect=AssertionError("Unexpected network request")), \
                patch.object(sys, "argv", ["build_web.py", "--build", str(self.build)]):
            build_web.main()
        self.assertEqual((self.output / "index.html").read_bytes(), b"local frontend")
        self.assertEqual((self.output / "native/relinker_wasm.wasm").read_bytes(), b"local wasm")

    def test_old_release_without_web_asset_preserves_the_previous_site(self):
        self.release["assets"] = [asset for asset in self.release["assets"] if asset["name"] != "web-v1.zip"]
        self.assert_rejected("publish a release with the web converter")

    def test_ambiguous_web_asset_is_rejected(self):
        self.release["assets"].append(dict(self.release["assets"][0]))
        self.assert_rejected("ambiguous release asset")

    def test_wrong_tag_or_invalid_revision_is_rejected(self):
        for metadata in ({"tag": "v2", "revision": REVISION}, {"tag": "v1", "revision": "main"},
                         {"tag": "v1", "revision": None}, []):
            with self.subTest(metadata=metadata):
                self.requests.clear()
                self.set_asset("web-v1.zip", zipped([*self.contents.items(), ("web-release.json", json.dumps(metadata))]))
                self.assert_rejected("does not match")

    def test_unsafe_duplicate_missing_or_empty_files_are_rejected(self):
        for entries in (self.entries + [("../escape", b"bad")], self.entries + [self.entries[0]],
                        self.entries[1:], [(name, b"" if name == "app.mjs" else data) for name, data in self.entries]):
            with self.subTest(names=[name for name, data in entries]):
                self.requests.clear()
                self.set_asset("web-v1.zip", zipped(entries))
                self.assert_rejected("Invalid.*web release archive")
                self.assertFalse((self.build / "escape").exists())

    def test_web_size_and_checksum_failures_preserve_previous_site(self):
        for failure in ("short", "long", "checksum", "limit"):
            with self.subTest(failure=failure):
                self.requests.clear()
                asset = self.set_asset("web-v1.zip", zipped(self.entries))
                if failure == "short":
                    self.responses[asset["browser_download_url"]] = b"short"
                elif failure == "long":
                    self.responses[asset["browser_download_url"]] += b"x"
                elif failure == "checksum":
                    asset["digest"] = "sha256:" + "0" * 64
                else:
                    asset["size"] = 128 * 1024 * 1024 + 1
                self.assert_rejected("(archive|checksum)")

    def test_missing_or_corrupt_library_never_publishes_the_new_converter(self):
        for failure in ("missing", "corrupt"):
            with self.subTest(failure=failure):
                self.requests.clear()
                self.set_asset("prx-linux-v1.zip", zipped([("libs/libkernel.prx", b"linux")]))
                if failure == "missing":
                    self.release["assets"] = [asset for asset in self.release["assets"] if asset["name"] != "prx-linux-v1.zip"]
                else:
                    self.set_asset("prx-linux-v1.zip", zipped([("../escape", b"bad")]))
                self.assert_rejected("(release asset|archive layout)")

    def test_library_download_failure_preserves_previous_site_and_removes_staging(self):
        self.seed_previous()
        previous = self.snapshot()

        def failed_request(url):
            if url.endswith("prx-linux-v1.zip"):
                raise OSError("connection lost")
            return self.request(url)

        with patch.object(build_web, "request", failed_request), self.assertRaisesRegex(OSError, "connection lost"):
            build_web.publish_release(self.output)
        self.assertEqual(self.snapshot(), previous)
        self.assertEqual(list(self.build.glob(".web-release-*")), [])

    def test_invalid_release_tag_is_rejected_before_downloading_assets(self):
        for tag in ("../v1", "v1."):
            with self.subTest(tag=tag):
                self.requests.clear()
                self.release["tag_name"] = tag
                self.assert_rejected("Invalid release tag")
                self.assertEqual(self.requests, [LATEST])

    def test_packaged_web_archive_is_accepted_by_the_publisher(self):
        local = self.root / "local/web"
        for name, content in self.contents.items():
            path = local / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
        dist = self.root / "dist"
        result = subprocess.run([sys.executable, str(Path(package_release.__file__)), "--platform", "web",
                                 "--build", str(local.parent), "--output", str(dist), "--version", "v1",
                                 "--revision", REVISION], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.set_asset("web-v1.zip", (dist / "web-v1.zip").read_bytes())
        self.publish()
        for name, content in self.contents.items():
            self.assertEqual((self.output / name).read_bytes(), content)


class WebPackageTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.build = self.root / "build"
        self.output = self.root / "dist"
        self.output.mkdir()
        self.asset = self.output / "web-v1.zip"
        self.asset.write_bytes(b"previous asset")
        for name in FILES:
            path = self.build / "web" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(name.encode())

    def test_missing_or_empty_web_file_preserves_previous_archive(self):
        file = self.build / "web/native/relinker_wasm.wasm"
        for empty in (False, True):
            with self.subTest(empty=empty):
                if empty:
                    file.write_bytes(b"")
                else:
                    file.unlink()
                with self.assertRaisesRegex(RuntimeError, "Missing or empty web release file"):
                    package_release.package_web(self.build, self.output, "v1", REVISION)
                self.assertEqual(self.asset.read_bytes(), b"previous asset")

    def test_package_includes_every_frontend_source(self):
        frontend = {file.name: file.read_bytes() for file in (package_release.ROOT / "demo/web").iterdir() if file.is_file()}
        for name, content in frontend.items():
            (self.build / "web" / name).write_bytes(content)
        package_release.package_web(self.build, self.output, "v1", REVISION)
        with zipfile.ZipFile(self.asset) as archive:
            self.assertEqual(set(archive.namelist()), {*frontend, "native/relinker_wasm.mjs", "native/relinker_wasm.wasm", "web-release.json"})
            for name, content in frontend.items():
                self.assertEqual(archive.read(name), content)

    def test_invalid_tag_or_revision_does_not_replace_existing_archive(self):
        for tag, revision in (("../v1", REVISION), ("v1.", REVISION), ("v1", "HEAD")):
            with self.subTest(tag=tag, revision=revision), self.assertRaises(ValueError):
                package_release.package_web(self.build, self.output, tag, revision)
        self.assertEqual(self.asset.read_bytes(), b"previous asset")

    def test_archive_write_failure_preserves_previous_archive(self):
        with patch.object(package_release.zipfile.ZipFile, "write", side_effect=OSError("disk full")), \
                self.assertRaisesRegex(OSError, "disk full"):
            package_release.package_web(self.build, self.output, "v1", REVISION)
        self.assertEqual(self.asset.read_bytes(), b"previous asset")
        self.assertEqual(list(self.output.glob(".release-*")), [])

    def test_cli_rejects_missing_or_misplaced_revision_without_writing_assets(self):
        base = [sys.executable, str(Path(package_release.__file__)), "--build", str(self.build),
                "--output", str(self.output), "--version", "v1"]
        for arguments in (["--platform", "web"], ["--platform", "linux", "--revision", REVISION]):
            with self.subTest(arguments=arguments):
                result = subprocess.run([*base, *arguments], capture_output=True, text=True)
                self.assertEqual(result.returncode, 2)
                self.assertIn("--revision", result.stderr)
                self.assertEqual(self.asset.read_bytes(), b"previous asset")


class TestWebRunnerTests(unittest.TestCase):
    def test_main_rejects_missing_archive_file_without_traceback(self):
        with tempfile.TemporaryDirectory() as tmp:
            build = Path(tmp) / "build"
            build.mkdir()
            with self.assertRaises(SystemExit) as error, contextlib.redirect_stderr(io.StringIO()) as err:
                test_web.main(["--build", str(build)])
            self.assertEqual(error.exception.code, 2)
            self.assertIn("archive module does not exist", err.getvalue())

    def test_main_rejects_archive_missing_zip_url(self):
        with tempfile.TemporaryDirectory() as tmp:
            build = Path(tmp) / "build"
            (build / "web").mkdir(parents=True)
            (build / "web/archive.mjs").write_text("const x = 1;\n", encoding="utf-8")
            with patch("subprocess.run"):
                with self.assertRaises(SystemExit) as error, contextlib.redirect_stderr(io.StringIO()) as err:
                    test_web.main(["--build", str(build)])
            self.assertEqual(error.exception.code, 2)
            self.assertIn("zip library URL not found", err.getvalue())

    def test_main_decodes_archive_with_utf8_bom(self):
        with tempfile.TemporaryDirectory() as tmp:
            build = Path(tmp) / "build"
            (build / "web").mkdir(parents=True)
            (build / "web/archive.mjs").write_text('\ufefffrom "https://cdn.jsdelivr.net/example/zip.mjs";\n', encoding="utf-8")
            with patch("subprocess.run"), \
                    patch("urllib.request.urlopen") as mock_url:
                mock_resp = mock_url.return_value.__enter__.return_value
                mock_resp.read.return_value = b"zip-content"
                result = test_web.main(["--build", str(build)])
                self.assertEqual(result, 0)

    def test_main_reconfigures_streams(self):
        class Stream:
            def __init__(self):
                self.encoding = None
                self.errors = None

            def reconfigure(self, encoding=None, errors=None):
                self.encoding = encoding
                self.errors = errors

            def write(self, text):
                return len(text)

            def flush(self):
                pass

        out, err = Stream(), Stream()
        with patch.object(test_web.sys, "stdout", out), patch.object(test_web.sys, "stderr", err):
            with self.assertRaises(SystemExit) as error:
                test_web.main([])
        self.assertEqual(error.exception.code, 2)
        self.assertEqual(out.encoding, "utf-8")
        self.assertEqual(out.errors, "backslashreplace")
        self.assertEqual(err.encoding, "utf-8")
        self.assertEqual(err.errors, "backslashreplace")


if __name__ == "__main__":
    unittest.main()
