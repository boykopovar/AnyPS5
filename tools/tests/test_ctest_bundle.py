import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import ctest_bundle
import sanitizer_report

TOOL = Path(__file__).resolve().parents[1] / "ctest_bundle.py"
GIT = shutil.which("git")
CMAKE = shutil.which("cmake")
CTEST = shutil.which("ctest")


class BundleFixture:
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="anyps5-ctest-bundle-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.source = self.root / "source"
        self.source.mkdir()
        self.directory = self.root / "bundle"
        self.environment = {
            "GITHUB_REPOSITORY": "owner/repository",
            "GITHUB_WORKFLOW": "Build",
            "GITHUB_WORKFLOW_REF": "owner/repository/.github/workflows/build.yml@refs/heads/main",
            "GITHUB_WORKFLOW_SHA": "a" * 40,
            "GITHUB_RUN_ID": "1234",
            "GITHUB_RUN_ATTEMPT": "1",
            "GITHUB_JOB": "relinker",
            "GITHUB_EVENT_NAME": "workflow_dispatch",
            "GITHUB_SHA": "b" * 40,
            "RUNNER_OS": "Windows",
            "RUNNER_ARCH": "X64",
            "CTEST_BASE_SHA": "c" * 40,
            "CTEST_PR_HEADS": "d" * 40 + " " + "e" * 40,
            "CTEST_PR_NUMBERS": "12 34",
            "CTEST_MATRIX_JSON": '{"os":"windows-latest","architectures":"x86_64"}',
            "CTEST_SELECTION": "all",
            "CTEST_REPEAT": "after-timeout:2",
            "CTEST_BUILD_DIRECTORY": "build-relinker",
        }

    def run_bundle(self, operation, *arguments, **keywords):
        previous = Path.cwd()
        try:
            os.chdir(self.source)
            return getattr(ctest_bundle, operation)(*arguments, **keywords)
        finally:
            os.chdir(previous)

    def prepare_fixture(self, directory=None, environment=None, output=None):
        return self.run_bundle("prepare", directory or self.directory,
                               environ=self.environment if environment is None else environment,
                               github_output=output)

    def read_manifest(self, published=False):
        parent = self.directory / "published" if published else self.directory
        return json.loads((parent / "manifest.json").read_text(encoding="utf-8"))

    def git(self, *arguments):
        if not GIT:
            self.skipTest("git is not available")
        return subprocess.run([GIT, "-c", "safe.directory=*", "-c", "user.name=fixture",
                               "-c", "user.email=fixture@example.invalid", *arguments],
                              cwd=self.source, capture_output=True, text=True,
                              encoding="utf-8", errors="replace", check=True, timeout=10).stdout.strip()

    def initialize_git(self):
        self.git("init", "--quiet")
        (self.source / "tracked.txt").write_bytes(b"initial\n")
        self.git("add", "tracked.txt")
        self.git("commit", "--quiet", "-m", "fixture")

    def make_symlink(self, target, link, directory=False):
        try:
            link.symlink_to(target, target_is_directory=directory)
        except (OSError, NotImplementedError) as error:
            self.skipTest(f"symlinks are unavailable: {error}")


class CTestBundlePrepareTests(BundleFixture, unittest.TestCase):
    def test_records_actual_git_commit_tree_and_ordered_pins(self):
        self.initialize_git()
        self.prepare_fixture()
        data = self.read_manifest()
        self.assertEqual(data["format"], "anyps5-ctest-bundle/1")
        self.assertEqual(data["phase"], "prepared")
        self.assertEqual(data["source"]["head"], self.git("rev-parse", "HEAD"))
        self.assertEqual(data["source"]["tree"], self.git("rev-parse", "HEAD^{tree}"))
        self.assertNotEqual(data["source"]["head"], data["ci"]["event_sha"])
        self.assertFalse(data["source"]["tracked_dirty"])
        self.assertEqual(data["test"]["base_sha"], "c" * 40)
        self.assertEqual(data["test"]["pr_heads"], ["d" * 40, "e" * 40])
        self.assertEqual(list(map(str, data["test"]["pr_numbers"])), ["12", "34"])
        self.assertEqual(data["matrix"], {"os": "windows-latest", "architectures": "x86_64"})
        self.assertIsNone(data["test"]["exit_code"])
        self.assertEqual(data["reports"], {})

    def test_tracked_edits_and_untracked_files_are_distinguished(self):
        self.initialize_git()
        (self.source / "untracked.txt").write_bytes(b"untracked\n")
        first = self.prepare_fixture()
        self.assertFalse(first["source"]["tracked_dirty"])
        (self.source / "tracked.txt").write_bytes(b"changed\n")
        second = self.prepare_fixture(directory=self.root / "changed-bundle")
        self.assertTrue(second["source"]["tracked_dirty"])
        self.assertEqual(first["source"]["head"], second["source"]["head"])
        self.assertEqual(first["source"]["tree"], second["source"]["tree"])

    def test_unknown_context_remains_unknown(self):
        data = self.prepare_fixture(environment={})
        self.assertIsNone(data["source"]["head"])
        self.assertIsNone(data["source"]["tree"])
        self.assertTrue(data["source"]["error"])
        self.assertIsNone(data["matrix"])
        for value in data["ci"].values():
            self.assertIsNone(value)
        self.assertIsNone(data["test"]["exit_code"])
        self.assertIsNone(data["test"]["step_outcome"])
        self.assertIsNone(data["test"]["selection"])

    def test_ignores_environment_secrets(self):
        environment = dict(self.environment, PRIVATE_FIXTURE_SECRET="do-not-publish-this-value")
        self.prepare_fixture(environment=environment)
        content = (self.directory / "manifest.json").read_text(encoding="utf-8")
        self.assertNotIn("PRIVATE_FIXTURE_SECRET", content)
        self.assertNotIn("do-not-publish-this-value", content)
        self.assertLessEqual(len(content.encode("utf-8")), ctest_bundle.MAX_MANIFEST_BYTES)

    def test_attempt_and_matrix_change_artifact_identity(self):
        names = []
        for index, updates in enumerate(({}, {"GITHUB_RUN_ATTEMPT": "2"},
                                         {"CTEST_MATRIX_JSON": '{"os":"macos-15","architectures":"x86_64"}'},
                                         {"CTEST_MATRIX_JSON": '{"os":"macos-15","architectures":"arm64"}'})):
            data = self.prepare_fixture(directory=self.root / f"bundle-{index}",
                                        environment=dict(self.environment, **updates))
            names.append(data["artifact_name"])
        self.assertEqual(len(set(names)), len(names))

    def test_dispatch_keeps_all_forty_ordered_pr_pins(self):
        heads = [f"{index:040x}" for index in range(1, 41)]
        numbers = [str(index) for index in range(1, 41)]
        result = self.prepare_fixture(environment=dict(self.environment,
            CTEST_PR_HEADS=" ".join(heads), CTEST_PR_NUMBERS=" ".join(numbers)))
        self.assertEqual(result["test"]["pr_heads"], heads)
        self.assertEqual(result["test"]["pr_numbers"], numbers)

    def test_configured_windows_toolchain_is_unknown_on_other_hosts(self):
        environment = dict(self.environment, RUNNER_OS="Linux", WINLIBS_RELEASE="release",
                           WINLIBS_ZIP="toolchain.zip", WINLIBS_SHA256="f" * 64)
        result = self.prepare_fixture(environment=environment)
        for name in ("winlibs_release", "winlibs_zip", "winlibs_sha256"):
            self.assertIsNone(result["toolchain"][name])
        self.assertTrue(result["toolchain"]["winlibs_identifiers_are_configured"])

    def test_metadata_inputs_are_bounded(self):
        for index, updates in enumerate(({"GITHUB_WORKFLOW": "x" * 2048},
                                         {"CTEST_MATRIX_JSON": '{"value":"' + "x" * 9000 + '"}'},
                                         {"CTEST_MATRIX_JSON": "not-json"},
                                         {"CTEST_MATRIX_JSON": "[]"})):
            with self.subTest(updates=next(iter(updates))):
                with self.assertRaises((ValueError, OSError)):
                    self.prepare_fixture(directory=self.root / f"invalid-{index}",
                                         environment=dict(self.environment, **updates))


class CTestBundleFinishTests(BundleFixture, unittest.TestCase):
    def test_preserves_original_bytes_hashes_and_only_allowlisted_files(self):
        self.prepare_fixture()
        originals = {"ctest.xml": b'<?xml version="1.0"?>\r\n<testsuite><testcase name="failure"><failure/></testcase></testsuite>\r\n',
                     "ctest.log": b"raw log\r\n\x00\xff\n"}
        for name, data in originals.items():
            (self.directory / name).write_bytes(data)
        (self.directory / "unrelated-secret.txt").write_bytes(b"not a report")
        result = self.run_bundle("finish", self.directory, "failure")
        self.assertEqual(result["phase"], "collected")
        self.assertEqual(result["test"]["step_outcome"], "failure")
        self.assertIsNone(result["test"]["exit_code"])
        self.assertEqual({path.name for path in (self.directory / "published").iterdir()},
                         {"manifest.json", "ctest.xml", "ctest.log"})
        for kind, name in (("junit", "ctest.xml"), ("log", "ctest.log")):
            record = result["reports"][kind]
            self.assertEqual(record["state"], "available_unvalidated")
            self.assertEqual(record["bytes"], len(originals[name]))
            self.assertEqual(record["sha256"], hashlib.sha256(originals[name]).hexdigest())
            self.assertEqual((self.directory / "published" / name).read_bytes(), originals[name])
            self.assertEqual((self.directory / name).read_bytes(), originals[name])
        self.assertEqual(self.read_manifest(published=True), result)

    def test_missing_report_never_claims_pass_for_any_step_outcome(self):
        for outcome in ("success", "failure", "skipped", "cancelled"):
            with self.subTest(outcome=outcome):
                directory = self.root / outcome
                self.prepare_fixture(directory=directory)
                result = self.run_bundle("finish", directory, outcome)
                self.assertEqual(result["test"]["step_outcome"], outcome)
                self.assertIsNone(result["test"]["exit_code"])
                for record in result["reports"].values():
                    self.assertEqual(record["state"], "missing")
                    self.assertIsNone(record["bytes"])
                    self.assertIsNone(record["sha256"])
                self.assertEqual({path.name for path in (directory / "published").iterdir()}, {"manifest.json"})
                self.assertNotIn("passed", json.dumps(result))

    def test_empty_and_malformed_xml_are_opaque_evidence(self):
        for index, content in enumerate((b"", b"not XML\x00\xff", b"<testsuite/>")):
            with self.subTest(content=content):
                directory = self.root / f"opaque-{index}"
                self.prepare_fixture(directory=directory)
                (directory / "ctest.xml").write_bytes(content)
                result = self.run_bundle("finish", directory, "failure")
                self.assertEqual(result["reports"]["junit"]["state"], "available_unvalidated")
                self.assertEqual((directory / "published/ctest.xml").read_bytes(), content)
                self.assertNotIn("tests_passed", result)

    def test_oversized_reports_are_explicitly_omitted_without_truncation(self):
        self.prepare_fixture()
        original = self.directory / "ctest.xml"
        with original.open("wb") as stream:
            stream.write(b"not truncated")
            stream.truncate(ctest_bundle.MAX_REPORT_BYTES + 1)
        result = self.run_bundle("finish", self.directory, "failure")
        self.assertEqual(result["reports"]["junit"]["state"], "omitted")
        self.assertTrue(result["reports"]["junit"]["reason"])
        self.assertFalse((self.directory / "published/ctest.xml").exists())
        self.assertEqual(original.stat().st_size, ctest_bundle.MAX_REPORT_BYTES + 1)
        with original.open("rb") as stream:
            self.assertEqual(stream.read(13), b"not truncated")

    def test_source_mutation_is_reported_at_collection(self):
        self.initialize_git()
        self.prepare_fixture()
        (self.source / "tracked.txt").write_bytes(b"later edit\n")
        self.git("add", "tracked.txt")
        self.git("commit", "--quiet", "-m", "later")
        result = self.run_bundle("finish", self.directory, "failure")
        self.assertFalse(result["source_matches_preparation"])
        self.assertNotEqual(result["source"]["head"], result["source_at_collection"]["head"])
        self.assertEqual(result["source_at_collection"]["head"], self.git("rev-parse", "HEAD"))

    def test_dirty_source_identity_cannot_claim_tree_match(self):
        self.initialize_git()
        (self.source / "tracked.txt").write_bytes(b"uncommitted change\n")
        self.prepare_fixture()
        result = self.run_bundle("finish", self.directory, "failure")
        self.assertTrue(result["source"]["tracked_dirty"])
        self.assertTrue(result["source_at_collection"]["tracked_dirty"])
        self.assertIsNone(result["source_matches_preparation"])

    def test_unknown_source_identity_cannot_claim_tree_match(self):
        self.prepare_fixture(environment={})
        result = self.run_bundle("finish", self.directory, "skipped")
        self.assertIsNone(result["source_matches_preparation"])

    def test_rejects_second_finish_and_oversized_manifest(self):
        self.prepare_fixture()
        self.run_bundle("finish", self.directory, "failure")
        with self.assertRaises((ValueError, OSError)):
            self.run_bundle("finish", self.directory, "failure")
        other = self.root / "other"
        self.prepare_fixture(directory=other)
        (other / "manifest.json").write_bytes(b" " * (ctest_bundle.MAX_MANIFEST_BYTES + 1))
        with self.assertRaises((ValueError, OSError)):
            self.run_bundle("finish", other, "failure")


class CTestBundlePathTests(BundleFixture, unittest.TestCase):
    def test_existing_directories_and_stale_reports_are_not_reused(self):
        self.directory.mkdir()
        stale = self.directory / "ctest.xml"
        stale.write_bytes(b'<testsuite><testcase name="old-pass"/></testsuite>')
        before = stale.read_bytes()
        with self.assertRaises((ValueError, OSError)):
            self.prepare_fixture()
        self.assertEqual(stale.read_bytes(), before)
        self.assertFalse((self.directory / "manifest.json").exists())
        empty = self.root / "empty"
        empty.mkdir()
        with self.assertRaises((ValueError, OSError)):
            self.prepare_fixture(directory=empty)

    def test_line_break_in_output_path_is_rejected(self):
        with self.assertRaises((ValueError, OSError)):
            self.prepare_fixture(directory=self.root / "bundle\ninjected=value")

    def test_spaces_and_unicode_paths_round_trip(self):
        self.directory = self.root / "bundle with spaces-\u03bb"
        self.prepare_fixture()
        (self.directory / "ctest.log").write_bytes(b"log\n")
        result = self.run_bundle("finish", self.directory, "success")
        self.assertEqual(Path(result["directory"]), self.directory)
        self.assertEqual((self.directory / "published/ctest.log").read_bytes(), b"log\n")

    def test_github_output_cannot_alias_bundle_files(self):
        for index, name in enumerate(("manifest.json", "ctest.xml", "ctest.log", "published/output")):
            directory = self.root / f"alias-{index}"
            with self.subTest(name=name), self.assertRaises((ValueError, OSError)):
                self.prepare_fixture(directory=directory, output=directory / name)

    def test_github_output_hardlink_preserves_original(self):
        original = self.root / "existing-output"
        original.write_bytes(b"preserve me\n")
        alias = self.root / "output-alias"
        try:
            os.link(original, alias)
        except OSError as error:
            self.skipTest(f"hardlinks are unavailable: {error}")
        with self.assertRaises((ValueError, OSError)):
            self.prepare_fixture(output=alias)
        self.assertEqual(original.read_bytes(), b"preserve me\n")
        self.assertFalse(self.directory.exists())

    def test_hardlinked_report_is_omitted(self):
        self.prepare_fixture()
        original = self.root / "outside-report"
        original.write_bytes(b"outside content")
        try:
            os.link(original, self.directory / "ctest.xml")
        except OSError as error:
            self.skipTest(f"hardlinks are unavailable: {error}")
        result = self.run_bundle("finish", self.directory, "failure")
        self.assertEqual(result["reports"]["junit"]["state"], "omitted")
        self.assertFalse((self.directory / "published/ctest.xml").exists())
        self.assertEqual(original.read_bytes(), b"outside content")

    def test_hardlinked_manifest_is_rejected_without_publication(self):
        self.prepare_fixture()
        path = self.directory / "manifest.json"
        original = path.read_bytes()
        try:
            os.link(path, self.root / "manifest-alias")
        except OSError as error:
            self.skipTest(f"hardlinks are unavailable: {error}")
        with self.assertRaises((ValueError, OSError)):
            self.run_bundle("finish", self.directory, "failure")
        self.assertEqual(path.read_bytes(), original)
        self.assertFalse((self.directory / "published").exists())

    def test_symlink_ancestor_is_rejected(self):
        target = self.root / "real-parent"
        target.mkdir()
        link = self.root / "linked-parent"
        self.make_symlink(target, link, directory=True)
        with self.assertRaises((ValueError, OSError)):
            self.prepare_fixture(directory=link / "bundle")
        self.assertFalse((target / "bundle").exists())

    def test_symlink_report_is_omitted_without_following_it(self):
        self.prepare_fixture()
        outside = self.root / "outside.xml"
        outside.write_bytes(b"outside private content")
        self.make_symlink(outside, self.directory / "ctest.xml")
        result = self.run_bundle("finish", self.directory, "failure")
        self.assertEqual(result["reports"]["junit"]["state"], "omitted")
        self.assertFalse((self.directory / "published/ctest.xml").exists())
        self.assertEqual(outside.read_bytes(), b"outside private content")

    def test_directory_in_place_of_report_is_omitted(self):
        self.prepare_fixture()
        (self.directory / "ctest.xml").mkdir()
        result = self.run_bundle("finish", self.directory, "failure")
        self.assertEqual(result["reports"]["junit"]["state"], "omitted")
        self.assertFalse((self.directory / "published/ctest.xml").exists())


class CTestBundleCliTests(BundleFixture, unittest.TestCase):
    def test_cli_writes_github_outputs_and_collects_failure(self):
        output = self.root / "github-output"
        environment = dict(os.environ, **self.environment)
        prepared = subprocess.run([sys.executable, str(TOOL), "prepare", "--directory", str(self.directory),
                                   "--github-output", str(output)], cwd=self.source, env=environment,
                                  capture_output=True, text=True, timeout=10)
        self.assertEqual(prepared.returncode, 0, prepared.stderr)
        outputs = dict(line.split("=", 1) for line in output.read_text(encoding="utf-8").splitlines())
        self.assertEqual(Path(outputs["directory"]), self.directory)
        self.assertEqual(Path(outputs["artifact_directory"]), self.directory / "published")
        self.assertTrue(outputs["artifact_name"])
        completed = subprocess.run([sys.executable, str(TOOL), "finish", "--directory", str(self.directory),
                                    "--outcome", "failure"], cwd=self.source, env=environment,
                                   capture_output=True, text=True, timeout=10)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(self.read_manifest(published=True)["test"]["step_outcome"], "failure")

    def test_cli_invalid_input_returns_two_and_preserves_existing_data(self):
        self.directory.mkdir()
        report = self.directory / "ctest.xml"
        report.write_bytes(b"old report")
        result = subprocess.run([sys.executable, str(TOOL), "prepare", "--directory", str(self.directory)],
                                cwd=self.source, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 2)
        self.assertTrue(result.stderr)
        self.assertEqual(report.read_bytes(), b"old report")
        missing = subprocess.run([sys.executable, str(TOOL), "finish", "--directory", str(self.root / "missing"),
                                  "--outcome", "failure"], cwd=self.source,
                                 capture_output=True, text=True, timeout=10)
        self.assertEqual(missing.returncode, 2)
        self.assertFalse((self.root / "missing").exists())

    def test_cli_rejects_malformed_preparation_manifest(self):
        for index, value in enumerate(([], None, {"format": "wrong"},
                                       {"format": "anyps5-ctest-bundle/1", "phase": "prepared",
                                        "source": "wrong", "directory": "placeholder"})):
            directory = self.root / f"invalid-manifest-{index}"
            directory.mkdir()
            if isinstance(value, dict) and value.get("directory"):
                value["directory"] = directory.as_posix()
            (directory / "manifest.json").write_text(json.dumps(value), encoding="utf-8")
            result = subprocess.run([sys.executable, str(TOOL), "finish", "--directory", str(directory),
                                     "--outcome", "failure"], cwd=self.source,
                                    capture_output=True, text=True, timeout=10)
            with self.subTest(value=value):
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertTrue(result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertFalse((directory / "published").exists())


class CTestFixture(BundleFixture):
    def configure_fixture(self):
        if not CMAKE or not CTEST:
            self.skipTest("CMake and CTest are required")
        script = """from pathlib import Path
import sys
import time

mode = sys.argv[1]
if mode in ('failed', 'fixture_setup'):
    print('expected failure')
    raise SystemExit(3)
if mode == 'skipped':
    raise SystemExit(77)
if mode == 'regex_skip':
    print('SKIP_THIS_FIXTURE')
if mode == 'timeout':
    print('timeout output', flush=True)
    time.sleep(2)
if mode.startswith('repeat_'):
    path = Path(sys.argv[2])
    count = int(path.read_text()) + 1 if path.exists() else 1
    path.write_text(str(count))
    print('attempt ' + str(count), flush=True)
    if mode == 'repeat_fail_second' and count >= 2:
        raise SystemExit(3)
    if mode == 'repeat_timeout_once' and count == 1:
        time.sleep(2)
"""
        (self.source / "case.py").write_bytes(script.encode("utf-8"))
        project = 'cmake_minimum_required(VERSION 3.20)\nproject(BundleFixture NONE)\nenable_testing()\n'
        for mode in ("passed", "failed", "skipped", "regex_skip", "timeout", "disabled", "fixture_setup", "fixture_consumer"):
            argument = "passed" if mode in ("disabled", "fixture_consumer") else mode
            project += f'add_test(NAME {mode} COMMAND "{Path(sys.executable).as_posix()}" "${{CMAKE_CURRENT_SOURCE_DIR}}/case.py" {argument})\n'
        project += 'set_tests_properties(skipped PROPERTIES SKIP_RETURN_CODE 77)\n'
        project += 'set_tests_properties(regex_skip PROPERTIES SKIP_REGULAR_EXPRESSION SKIP_THIS_FIXTURE)\n'
        project += 'set_tests_properties(timeout PROPERTIES TIMEOUT 0.25)\n'
        project += 'set_tests_properties(disabled PROPERTIES DISABLED TRUE)\n'
        project += 'set_tests_properties(fixture_setup PROPERTIES FIXTURES_SETUP broken)\n'
        project += 'set_tests_properties(fixture_consumer PROPERTIES FIXTURES_REQUIRED broken)\n'
        project += 'add_test(NAME missing COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/missing-program")\n'
        for mode in ("repeat_stable", "repeat_fail_second", "repeat_timeout_once"):
            project += f'add_test(NAME {mode} COMMAND "{Path(sys.executable).as_posix()}" "${{CMAKE_CURRENT_SOURCE_DIR}}/case.py" {mode} "${{CMAKE_CURRENT_BINARY_DIR}}/{mode}.count")\n'
        project += 'set_tests_properties(repeat_timeout_once PROPERTIES TIMEOUT 0.25)\n'
        (self.source / "CMakeLists.txt").write_bytes(project.encode("utf-8"))
        self.build = self.root / "build"
        completed = subprocess.run([CMAKE, "-S", str(self.source), "-B", str(self.build)],
                                   capture_output=True, text=True, timeout=15)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)

    def ctest(self, arguments, report=None, log=None, directory=None):
        command = [CTEST, "--test-dir", str(directory or self.build), "--output-on-failure", "--no-tests=error", *arguments]
        if report is not None:
            command.extend(["--output-junit", str(report)])
        if log is not None:
            command.extend(["--output-log", str(log)])
        return subprocess.run(command, capture_output=True, text=True, encoding="utf-8",
                              errors="replace", timeout=15)


class CTestBundleCMakeTests(CTestFixture, unittest.TestCase):
    def test_nine_outcomes_keep_original_exit_and_xml(self):
        self.configure_fixture()
        pattern = "^(passed|failed|skipped|regex_skip|timeout|disabled|missing|fixture_setup|fixture_consumer)$"
        plain = self.ctest(["-R", pattern, "--parallel", "1"])
        self.prepare_fixture()
        report = self.directory / "ctest.xml"
        log = self.directory / "ctest.log"
        reported = self.ctest(["-R", pattern, "--parallel", "1"], report, log)
        self.assertEqual(plain.returncode, reported.returncode)
        self.assertEqual(reported.returncode, 8)
        self.assertEqual(sanitizer_report.results(report), {
            "passed": "passed", "failed": "failed", "skipped": "skipped", "regex_skip": "skipped",
            "timeout": "failed", "disabled": "skipped", "missing": "failed",
            "fixture_setup": "failed", "fixture_consumer": "failed",
        })
        original_xml, original_log = report.read_bytes(), log.read_bytes()
        result = self.run_bundle("finish", self.directory, "failure")
        self.assertEqual(result["test"]["step_outcome"], "failure")
        self.assertEqual((self.directory / "published/ctest.xml").read_bytes(), original_xml)
        self.assertEqual((self.directory / "published/ctest.log").read_bytes(), original_log)

    def test_zero_selection_is_retained_without_inventing_pass(self):
        self.configure_fixture()
        self.prepare_fixture()
        report = self.directory / "ctest.xml"
        result = self.ctest(["-R", "^no_match$"], report)
        self.assertEqual(result.returncode, 8)
        self.assertEqual(list(ET.parse(report).getroot().iter("testcase")), [])
        collected = self.run_bundle("finish", self.directory, "failure")
        self.assertEqual(collected["reports"]["junit"]["state"], "available_unvalidated")
        self.assertEqual(collected["test"]["step_outcome"], "failure")
        self.assertEqual((self.directory / "published/ctest.xml").read_bytes(), report.read_bytes())

    def test_invalid_directory_cannot_publish_old_pass_as_new_attempt(self):
        self.configure_fixture()
        old = self.root / "old-passing.xml"
        self.assertEqual(self.ctest(["-R", "^passed$"], old).returncode, 0)
        original = old.read_bytes()
        invalid = self.ctest([], old, directory=self.root / "invalid-build")
        self.assertNotEqual(invalid.returncode, 0)
        self.assertEqual(old.read_bytes(), original)
        stale_directory = self.root / "stale-bundle"
        stale_directory.mkdir()
        (stale_directory / "ctest.xml").write_bytes(original)
        with self.assertRaises((ValueError, OSError)):
            self.prepare_fixture(directory=stale_directory)
        self.prepare_fixture(environment=dict(self.environment, GITHUB_RUN_ATTEMPT="2"))
        fresh = self.ctest([], self.directory / "ctest.xml", directory=self.root / "invalid-build")
        self.assertNotEqual(fresh.returncode, 0)
        collected = self.run_bundle("finish", self.directory, "failure")
        self.assertEqual(collected["reports"]["junit"]["state"], "missing")
        self.assertFalse((self.directory / "published/ctest.xml").exists())


class CTestBundleRepeatTests(CTestFixture, unittest.TestCase):
    def test_repeated_success_failure_and_timeout_recovery_keep_policy(self):
        self.configure_fixture()
        for index, (mode, repeat, attempts, code) in enumerate((
                ("repeat_stable", "until-fail:3", 3, 0),
                ("repeat_fail_second", "until-fail:3", 2, 8),
                ("repeat_timeout_once", "after-timeout:2", 2, 0))):
            with self.subTest(mode=mode):
                directory = self.root / f"repeated-{index}"
                self.prepare_fixture(directory=directory, environment=dict(self.environment, CTEST_REPEAT=repeat))
                counter = self.build / f"{mode}.count"
                plain = self.ctest(["-R", f"^{mode}$", "--repeat", repeat])
                self.assertEqual(plain.returncode, code)
                self.assertEqual(int(counter.read_text()), attempts)
                counter.unlink()
                report = directory / "ctest.xml"
                reported = self.ctest(["-R", f"^{mode}$", "--repeat", repeat], report, directory / "ctest.log")
                self.assertEqual(reported.returncode, code)
                self.assertEqual(int(counter.read_text()), attempts)
                self.assertEqual(len(list(ET.parse(report).getroot().iter("testcase"))), 1)
                collected = self.run_bundle("finish", directory, "success" if code == 0 else "failure")
                self.assertEqual(collected["test"]["repeat"], repeat)
                self.assertNotIn("attempts", collected["test"])
                self.assertEqual((directory / "published/ctest.xml").read_bytes(), report.read_bytes())


if __name__ == "__main__":
    unittest.main()
