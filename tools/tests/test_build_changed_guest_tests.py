import contextlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_changed_guest_tests


def run_result(stdout=""):
    return mock.Mock(returncode=0, stdout=stdout, stderr="")


class GuestTestTargets(unittest.TestCase):
    def target(self, name, target_type="EXECUTABLE", source="core/libs/tests/Guest.cpp", fragment=""):
        return {
            "artifacts": [{"path": f"tests/{name}.exe"}],
            "id": name,
            "link": {"commandFragments": [{"fragment": fragment}] if fragment else []},
            "name": name,
            "paths": {"build": "core/libs", "source": "core/libs"},
            "sources": [{"path": source}],
            "type": target_type,
        }

    def fixture(self, targets):
        directory = Path(self.temporary.name)
        reply = directory / "build" / ".cmake" / "api" / "v1" / "reply"
        reply.mkdir(parents=True)
        entries = []
        for target in targets:
            filename = f"target-{target['name']}.json"
            (reply / filename).write_text(json.dumps(target), encoding="utf-8")
            entries.append({"id": target["id"], "jsonFile": filename, "name": target["name"]})
        model = {"configurations": [{"targets": entries}], "paths": {"source": str(directory)}}
        (reply / "codemodel.json").write_text(json.dumps(model), encoding="utf-8")
        index = {"reply": {"codemodel-v2": {"jsonFile": "codemodel.json"}}}
        (reply / "index-0001.json").write_text(json.dumps(index), encoding="utf-8")
        return directory / "build"

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.temporary.cleanup()

    def selections(self, build, changed):
        source_root, targets = build_changed_guest_tests.host_only.read_reply(build.resolve())
        return build_changed_guest_tests.guest_test_targets(source_root, targets, changed)

    def test_selects_core_libs_test_executables_with_guest_libc(self):
        guest = self.target("guest_pad_tests", fragment="libc.prx")
        host = self.target("host_tests")
        product = self.target("libScePad", target_type="SHARED_LIBRARY", source="core/libs/prx/libScePad/Export.cpp")
        self.assertEqual(self.selections(self.fixture([guest, host, product]), None), ["guest_pad_tests"])

    def test_selects_changed_guest_test_with_mingw_libc_import_library(self):
        for fragment in (
            "core/libs/libs/implib/liblibc.dll.a",
            r'"C:\build with spaces\core\libs\libs\implib\liblibc.dll.a"',
        ):
            with self.subTest(fragment=fragment):
                guest = self.target("guest_mktime_tests", source="core/libs/tests/GuestMktime.cpp", fragment=fragment)
                targets = {guest["id"]: guest}
                source_root = Path(self.temporary.name).resolve()
                for changed in (["core/libs/tests/GuestMktime.cpp"], ["core/libs/prx/libc/include/SonyCrt.hpp"]):
                    self.assertEqual(
                        build_changed_guest_tests.guest_test_targets(source_root, targets, changed),
                        ["guest_mktime_tests"],
                    )

    def test_changed_source_selects_only_the_changed_target(self):
        guest_a = self.target("guest_a_tests", source="core/libs/tests/GuestA.cpp", fragment="libc.prx")
        guest_b = self.target("guest_b_tests", source="core/libs/tests/GuestB.cpp", fragment="libc.prx")
        self.assertEqual(self.selections(self.fixture([guest_a, guest_b]), ["core/libs/tests/GuestB.cpp"]), ["guest_b_tests"])

    def test_a_production_source_change_selects_no_test_but_reaches_the_run(self):
        guest = self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")
        self.assertEqual(self.selections(self.fixture([guest]), ["core/libs/prx/libc/src/FloatBuiltins.cpp"]), [])

    def test_a_header_change_selects_every_guest_test(self):
        guest_a = self.target("guest_a_tests", source="core/libs/tests/GuestA.cpp", fragment="libc.prx")
        guest_b = self.target("guest_b_tests", source="core/libs/tests/GuestB.cpp", fragment="libc.prx")
        self.assertEqual(self.selections(self.fixture([guest_a, guest_b]), ["core/libs/prx/libScePad/include/Pad.hpp"]), ["guest_a_tests", "guest_b_tests"])

    def test_a_cmake_fragment_change_selects_every_guest_test(self):
        guest_a = self.target("guest_a_tests", source="core/libs/tests/GuestA.cpp", fragment="libc.prx")
        guest_b = self.target("guest_b_tests", source="core/libs/tests/GuestB.cpp", fragment="libc.prx")
        fragment = "core/libs/prx/libSceAgcDriver/tests/cmake/agc_driver_flip_tests.cmake"
        self.assertEqual(self.selections(self.fixture([guest_a, guest_b]), [fragment]), ["guest_a_tests", "guest_b_tests"])

    def test_a_build_wide_input_selects_every_guest_test_and_reaches_the_libs(self):
        guest_a = self.target("guest_a_tests", source="core/libs/tests/GuestA.cpp", fragment="libc.prx")
        guest_b = self.target("guest_b_tests", source="core/libs/tests/GuestB.cpp", fragment="libc.prx")
        build = self.fixture([guest_a, guest_b])
        for path in ("3rdparty/ZArchive", "cmake/Opus.cmake", "core/CMakeLists.txt", ".gitmodules"):
            with self.subTest(path=path):
                self.assertTrue(build_changed_guest_tests.reaches_libs({path}))
                self.assertEqual(self.selections(build, [path]), ["guest_a_tests", "guest_b_tests"])

    def test_a_prx_change_runs_the_test_that_loads_every_built_library(self):
        guest = self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")
        loader = self.target("patched_libraries_tests", source="core/libs/tests/PatchedLibraries.cpp")
        build = self.fixture([guest, loader])
        for path in ("core/libs/prx/libcairo/Export.cpp", "core/libs/prx/libcairo/CMakeLists.txt"):
            with self.subTest(path=path):
                self.assertIn("patched_libraries_tests", self.selections(build, [path]))
        self.assertEqual(self.selections(build, ["core/libs/prx/libcairo/Export.cpp"]), ["patched_libraries_tests"])
        self.assertEqual(self.selections(build, ["core/libs/prx/libScePad/include/Pad.hpp"]), ["guest_pad_tests", "patched_libraries_tests"])
        self.assertEqual(self.selections(build, ["3rdparty/ZArchive"]), ["guest_pad_tests", "patched_libraries_tests"])

    def test_a_change_that_cannot_alter_a_built_library_does_not_run_the_loader(self):
        guest = self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")
        loader = self.target("patched_libraries_tests", source="core/libs/tests/PatchedLibraries.cpp")
        build = self.fixture([guest, loader])
        self.assertEqual(self.selections(build, ["core/libs/tests/Guest.cpp"]), ["guest_pad_tests"])
        self.assertEqual(self.selections(build, ["core/relinker/codegen/src/Amd64OnlyConverter.cpp"]), [])

    def test_an_unrelated_change_selects_nothing(self):
        guest = self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")
        self.assertEqual(self.selections(self.fixture([guest]), ["core/relinker/codegen/src/Amd64OnlyConverter.cpp"]), [])


class OwnerResolution(unittest.TestCase):
    def test_cmake_owner_marker_precedes_tool_options_and_client_arguments(self):
        owner = build_changed_guest_tests.test_owner(
            ["C:/cmake.exe", "-DPATCHER=C:/b/nid_patcher.exe", "-P", "wrapper.cmake", "--",
             "C:/b/guest_owner_tests.exe", "C:/b/guest_client_tests.exe"],
            {"nid_patcher": "nid_patcher", "guest_owner_tests": "guest_owner_tests",
             "guest_client_tests": "guest_client_tests"})
        self.assertEqual(owner, "guest_owner_tests")

    def test_cmake_owner_marker_also_resolves_extensionless_executables(self):
        owner = build_changed_guest_tests.test_owner(
            ["/usr/bin/cmake", "-P", "wrapper.cmake", "--", "/build/guest_owner_tests", "/build/guest_client_tests"],
            {"guest_owner_tests": "guest_owner_tests", "guest_client_tests": "guest_client_tests"})
        self.assertEqual(owner, "guest_owner_tests")

    def test_resolves_the_executable_not_a_test_argument(self):
        owner = build_changed_guest_tests.test_owner(
            ["C:/b/tests/guest_np_common_tests.exe", "build"], {"guest_np_common_tests": "guest_np_common_tests"})
        self.assertEqual(owner, "guest_np_common_tests")

    def test_cmake_wrapper_without_marker_resolves_an_extensionless_path(self):
        owner = build_changed_guest_tests.test_owner(
            ["/usr/bin/cmake", "-DPATCHER=/build/patcher", "-P", "wrapper.cmake", "/build/host_tests", "write"],
            {"patcher": "patcher", "host_tests": "host_tests"})
        self.assertEqual(owner, "host_tests")

    def test_argumentless_registration_resolves(self):
        owner = build_changed_guest_tests.test_owner(
            ["C:/b/tests/guest_pad_tests.exe"], {"guest_pad_tests": "guest_pad_tests"})
        self.assertEqual(owner, "guest_pad_tests")

    def test_an_argument_that_merely_looks_like_a_target_does_not_own_the_test(self):
        owner = build_changed_guest_tests.test_owner(
            ["C:/b/tests/other_tests.exe", "guest_pad_tests"], {"guest_pad_tests": "guest_pad_tests"})
        self.assertIsNone(owner)


class Main(unittest.TestCase):
    def target(self, name, target_type="EXECUTABLE", source="core/libs/tests/Guest.cpp", fragment=""):
        return {
            "artifacts": [{"path": f"tests/{name}.exe"}],
            "id": name,
            "link": {"commandFragments": [{"fragment": fragment}] if fragment else []},
            "name": name,
            "paths": {"build": "core/libs", "source": "core/libs"},
            "sources": [{"path": source}],
            "type": target_type,
        }

    def listing(self):
        return json.dumps({"tests": [
            {"name": "guest_np_common_build", "command": ["C:/b/tests/guest_np_common_tests.exe", "build"]},
            {"name": "guest_np_common_fetch", "command": ["C:/b/tests/guest_np_common_tests.exe", "fetch"]},
            {"name": "guest_pad", "command": ["C:/b/tests/guest_pad_tests.exe"]},
        ]})

    def fixture(self, targets):
        directory = Path(self.temporary.name)
        reply = directory / "build" / ".cmake" / "api" / "v1" / "reply"
        reply.mkdir(parents=True)
        entries = []
        for target in targets:
            filename = f"target-{target['name']}.json"
            (reply / filename).write_text(json.dumps(target), encoding="utf-8")
            entries.append({"id": target["id"], "jsonFile": filename, "name": target["name"]})
        model = {"configurations": [{"targets": entries}], "paths": {"source": str(directory)}}
        (reply / "codemodel.json").write_text(json.dumps(model), encoding="utf-8")
        index = {"reply": {"codemodel-v2": {"jsonFile": "codemodel.json"}}}
        (reply / "index-0001.json").write_text(json.dumps(index), encoding="utf-8")
        return directory / "build"

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.temporary.cleanup()

    def main(self, build, changed_lines):
        changed = Path(self.temporary.name) / "changed.txt"
        changed.write_text("\n".join(changed_lines), encoding="utf-8")
        return build_changed_guest_tests.main([str(build), str(changed)])

    def test_mixed_argument_bearing_and_argumentless_tests_all_run(self):
        build = self.fixture([
            self.target("guest_np_common_tests", source="core/libs/tests/GuestNp.cpp", fragment="libc.prx"),
            self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx"),
        ])
        calls = []
        selections = []
        def execute(cmd, **_):
            calls.append(cmd)
            if "-I" in cmd:
                selections.append(Path(cmd[cmd.index("-I") + 1]).read_text())
            return run_result(self.listing() if "--show-only=json-v1" in cmd else "")
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute):
            code = self.main(build, ["core/libs/tests/GuestNp.cpp", "core/libs/tests/Guest.cpp"])
        self.assertEqual(code, 0)
        run_cmd = [cmd for cmd in calls if cmd[0] == "ctest" and not any("--show-only" in part for part in cmd)][0]
        self.assertEqual(selections, ["0,0,0,1,2,3\n"])
        self.assertIn("--no-tests=error", run_cmd)

    def test_cmake_wrapper_is_not_owned_by_its_guest_client_argument(self):
        build = self.fixture([
            self.target("guest_http_client_tests", source="core/libs/tests/HttpClient.cpp", fragment="libc.prx"),
            self.target("guest_avplayer_tests", source="core/libs/tests/AvPlayer.cpp", fragment="libc.prx"),
            self.target("avplayer_http_patched_modules_tests", source="core/libs/tests/AvPlayerHttpModules.cpp"),
            self.target("nid_patcher", source="core/nid_patcher/main.cpp"),
        ])
        listing = json.dumps({"tests": [
            {"name": "guest_http_client", "command": ["C:/b/tests/guest_http_client_tests.exe"]},
            {"name": "guest_avplayer", "command": ["C:/b/tests/guest_avplayer_tests.exe"]},
            {"name": "avplayer_http_patched_modules", "command": [
                "C:/cmake/bin/cmake.exe", "-DPATCHER=C:/b/nid_patcher.exe",
                "-P", "AvPlayerHttpModules.cmake", "--",
                "C:/b/tests/avplayer_http_patched_modules_tests.exe",
                "C:/b/tests/guest_http_client_tests.exe", "C:/b/tests/guest_avplayer_tests.exe",
            ]},
            {"name": "public_headers", "command": ["C:/python.exe", "test_public_headers.py"]},
        ]})
        selections = []
        builds = []
        def execute(cmd, **_):
            if cmd[0] == "cmake":
                builds.append(cmd[cmd.index("--target") + 1:])
            if "-I" in cmd:
                selections.append(Path(cmd[cmd.index("-I") + 1]).read_text())
            return run_result(listing if "--show-only=json-v1" in cmd else "")
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute):
            code = self.main(build, ["core/libs/include/Changed.hpp"])
        self.assertEqual(code, 0)
        self.assertEqual(selections, ["0,0,0,1,2,3,4\n"])
        self.assertTrue(any("avplayer_http_patched_modules_tests" in batch for batch in builds))

    def test_a_header_change_also_runs_the_public_header_tests(self):
        build = self.fixture([self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")])
        listing = json.dumps({"tests": [
            {"name": "guest_pad", "command": ["C:/b/tests/guest_pad_tests.exe"]},
            {"name": "public_headers", "command": ["C:/python.exe", "test_public_headers.py"]},
            {"name": "public_headers_compile_0", "command": ["C:/python.exe", "test_public_headers.py"]},
            {"name": "public_headers_compile_63", "command": ["C:/python.exe", "test_public_headers.py"]},
            {"name": "public_headers_manifest_other", "command": ["C:/python.exe", "x.py"]},
        ]})
        selections = []
        def execute(cmd, **_):
            if "-I" in cmd:
                selections.append(Path(cmd[cmd.index("-I") + 1]).read_text())
            return run_result(listing if "--show-only=json-v1" in cmd else "")
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute):
            self.assertEqual(self.main(build, ["core/libs/prx/libSceXml/include/Xml.hpp"]), 0)
        self.assertEqual(selections, ["0,0,0,1,2,3,4\n"])

    def test_a_production_source_change_does_not_run_the_public_header_tests(self):
        build = self.fixture([self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")])
        listing = json.dumps({"tests": [{"name": "public_headers", "command": ["C:/python.exe", "x.py"]}]})
        selections = []
        def execute(cmd, **_):
            if "-I" in cmd:
                selections.append(Path(cmd[cmd.index("-I") + 1]).read_text())
            return run_result(listing if "--show-only=json-v1" in cmd else "")
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute):
            self.assertEqual(self.main(build, ["core/libs/prx/libSceXml/src/Xml.cpp"]), 0)
        self.assertEqual(selections, [])

    def test_a_missing_public_header_registration_is_an_error(self):
        build = self.fixture([self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")])
        listing = json.dumps({"tests": [{"name": "guest_pad", "command": ["C:/b/tests/guest_pad_tests.exe"]}]})
        def execute(cmd, **_):
            return run_result(listing if "--show-only=json-v1" in cmd else "")
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(self.main(build, ["core/libs/prx/libSceXml/include/Xml.hpp"]), 1)

    def test_public_header_inputs(self):
        for path in ("core/libs/prx/libSceXml/include/Xml.hpp", "core/libs/include/X.h", "core/libs/tests/test_public_headers.py",
                     "tools/nid_names.py", "core/libs/CMakeLists.txt", "3rdparty/ZArchive"):
            with self.subTest(path=path):
                self.assertTrue(build_changed_guest_tests.checks_public_headers([path]))
        for path in ("core/libs/prx/libSceXml/src/Xml.cpp", "core/libs/tests/Guest.cpp", "core/relinker/include/R.hpp", "README.md"):
            with self.subTest(path=path):
                self.assertFalse(build_changed_guest_tests.checks_public_headers([path]))

    def test_output_log_reaches_the_ctest_run(self):
        build = self.fixture([self.target("guest_pad_tests", fragment="libc.prx")])
        changed = Path(self.temporary.name) / "changed.txt"
        changed.write_text("core/libs/tests/Guest.cpp", encoding="utf-8")
        calls = []
        def execute(cmd, **_):
            calls.append(cmd)
            return run_result(self.listing() if "--show-only=json-v1" in cmd else "")
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute):
            code = build_changed_guest_tests.main([str(build), str(changed), "--output-log", "ctest.log"])
        self.assertEqual(code, 0)
        run_cmd = [cmd for cmd in calls if cmd[0] == "ctest" and "-I" in cmd]
        self.assertEqual(len(run_cmd), 1)
        self.assertEqual(run_cmd[0][run_cmd[0].index("--output-log") + 1], "ctest.log")

    def test_large_selection_fits_windows_commands_and_preserves_every_test(self):
        selected = ["guest_" + "long_name_" * 8 + str(index) for index in range(600)]
        tests = [{"name": "unrelated", "command": ["other.exe"]}]
        tests += [{"name": f"case [{index}].+ \U0001f600", "command": [f"C:/b/{name}.exe"]}
                  for index, name in enumerate(selected)]
        listing = json.dumps({"tests": tests})
        calls = []
        seen_indices = []
        def execute(cmd, **_):
            length = len(subprocess.list2cmdline(cmd).encode("utf-16-le")) // 2
            if length >= 32767:
                raise OSError(206, "Windows command line is too long")
            calls.append(cmd)
            if "-I" in cmd:
                content = Path(cmd[cmd.index("-I") + 1]).read_text()
                seen_indices.extend(map(int, content.split(",")[3:]))
            return run_result(listing if "--show-only=json-v1" in cmd else "")
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute), \
                mock.patch("select_tests.sys.platform", "win32"):
            build_changed_guest_tests.run(Path(self.temporary.name), selected, {name: name for name in selected})
        builds = [cmd[cmd.index("--target") + 1:] for cmd in calls if cmd[0] == "cmake"]
        self.assertEqual(builds[0], ["libs"])
        self.assertGreater(len(builds), 2)
        self.assertEqual([name for batch in builds[1:] for name in batch], selected)
        self.assertEqual(seen_indices, list(range(2, len(tests) + 1)))
        self.assertEqual(len([cmd for cmd in calls if "-I" in cmd]), 1)

    @unittest.skipUnless(shutil.which("ctest"), "CTest is required to run the selected tests")
    def test_ctest_index_file_runs_every_resolved_test(self):
        build = Path(self.temporary.name) / "ctest-build"
        build.mkdir()
        python = Path(sys.executable).as_posix()
        body = 'add_test("unrelated" "cmake" "-E" "touch" "unrelated")\n'
        for index in range(11):
            body += f'add_test("selected [{index}].+" "{python}" "-c" "open(\'ran-{index}\', \'w\')")\n'
        (build / "CTestTestfile.cmake").write_text(body)
        real_run = subprocess.run
        def execute(cmd, **kwargs):
            if cmd[0] == "cmake":
                return run_result()
            return real_run(cmd, **{**kwargs, "capture_output": True})
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute):
            target = {"name": "python_target", "artifacts": [{"path": python}]}
            build_changed_guest_tests.run(build, ["python_target"], build_changed_guest_tests.target_artifact_names(target))
        self.assertEqual(sorted(path.name for path in build.glob("ran-*")), sorted(f"ran-{index}" for index in range(11)))
        self.assertFalse((build / "unrelated").exists())

    def test_changed_host_fixture_runs_its_cases_and_propagates_failure(self):
        host = self.target("host_tests", source="core/libs/tests/Host.cpp")
        python = Path(sys.executable).as_posix()
        host["artifacts"] = [{"path": python}]
        build = self.fixture([host])
        real_run = subprocess.run
        builds = []

        def execute(cmd, **kwargs):
            if cmd[0] == "cmake":
                builds.append(cmd[cmd.index("--target") + 1:])
                return run_result()
            return real_run(cmd, **{**kwargs, "capture_output": True})

        for failing in (False, True):
            with self.subTest(failing=failing):
                body = 'add_test("unrelated" "cmake" "-E" "touch" "unrelated")\n'
                for index in range(2):
                    marker = f"ran-host-{index}"
                    (build / marker).unlink(missing_ok=True)
                    status = int(failing and index == 1)
                    code = f"open('{marker}', 'w').close(); raise SystemExit({status})"
                    body += f'add_test("host [{index}].+" "{python}" "-c" "{code}")\n'
                (build / "CTestTestfile.cmake").write_text(body)
                with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute):
                    result = self.main(build, ["core/libs/tests/Host.cpp"])
                self.assertEqual(result, int(failing))
                self.assertTrue(all((build / f"ran-host-{index}").exists() for index in range(2)))
                self.assertFalse((build / "unrelated").exists())
        self.assertEqual(builds, [["libs"], ["host_tests"], ["libs"], ["host_tests"]])

    def test_a_production_only_change_still_builds_libs_and_runs_nothing(self):
        build = self.fixture([self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")])
        calls = []
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=lambda cmd, **_: calls.append(cmd) or run_result()):
            code = self.main(build, ["core/libs/prx/libc/src/FloatBuiltins.cpp"])
        self.assertEqual(code, 0)
        self.assertIn(["cmake", "--build", str(build), "--parallel", "3", "--target", "libs"], calls)
        self.assertFalse([cmd for cmd in calls if cmd[0] == "ctest"])

    def test_selected_target_with_a_renamed_executable_runs_its_ctest(self):
        guest = self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")
        guest["artifacts"] = [{"path": "tests/pad-renamed.exe"}]
        build = self.fixture([guest])
        listing = json.dumps({"tests": [{"name": "guest_pad", "command": ["C:/b/tests/pad-renamed.exe"]}]})
        calls = []
        selections = []
        def execute(cmd, **_):
            calls.append(cmd)
            if "-I" in cmd:
                selections.append(Path(cmd[cmd.index("-I") + 1]).read_text())
            return run_result(listing if "--show-only=json-v1" in cmd else "")
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute):
            code = self.main(build, ["core/libs/tests/Guest.cpp"])
        self.assertEqual(code, 0)
        commands = [cmd for cmd in calls if cmd[0] == "ctest" and "--show-only=json-v1" not in cmd]
        self.assertEqual(len(commands), 1)
        self.assertEqual(selections, ["0,0,0,1\n"])

    def test_an_unrelated_change_runs_nothing(self):
        build = self.fixture([self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")])
        calls = []
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=lambda cmd, **_: calls.append(cmd) or run_result()):
            code = self.main(build, ["core/relinker/codegen/src/Amd64OnlyConverter.cpp"])
        self.assertEqual(code, 0)
        self.assertEqual(calls, [])

    def test_partial_target_to_test_coverage_is_rejected(self):
        build = self.fixture([
            self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx"),
            self.target("guest_unregistered_tests", source="core/libs/tests/GuestUnregistered.cpp", fragment="libc.prx"),
        ])
        calls = []
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=lambda cmd, **_: calls.append(cmd) or run_result(self.listing() if "--show-only=json-v1" in cmd else "")):
            code = self.main(build, ["core/libs/tests/Guest.cpp", "core/libs/tests/GuestUnregistered.cpp"])
        self.assertEqual(code, 1)
        self.assertFalse([cmd for cmd in calls if cmd[0] == "ctest" and not any("--show-only" in part for part in cmd)])

    def test_a_ctest_failure_propagates(self):
        build = self.fixture([self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")])
        def fail(cmd, **_):
            if cmd[0] == "ctest" and "--show-only=json-v1" not in cmd:
                raise subprocess.CalledProcessError(2, cmd)
            return run_result(self.listing() if "--show-only=json-v1" in cmd else "")
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=fail):
            code = self.main(build, ["core/libs/tests/Guest.cpp"])
        self.assertEqual(code, 1)

    def test_a_libs_build_failure_propagates(self):
        build = self.fixture([self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")])
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=subprocess.CalledProcessError(2, "cmake")):
            code = self.main(build, ["core/libs/prx/libc/src/FloatBuiltins.cpp"])
        self.assertEqual(code, 1)

    def test_symbolic_link_loop_build_directory_is_reported_cleanly(self):
        root = Path(self.temporary.name)
        loop = root / "build"
        try:
            loop.symlink_to(root / "other")
            (root / "other").symlink_to(loop)
        except (NotImplementedError, OSError) as error:
            self.skipTest(f"symbolic links are unavailable: {error}")
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors):
            self.assertEqual(build_changed_guest_tests.main([str(loop)]), 1)
        self.assertIn("no CMake File API index", errors.getvalue())
        self.assertNotIn("Traceback", errors.getvalue())


class ArtifactOwnerNames(unittest.TestCase):
    def test_dotted_artifact_names_keep_non_executable_suffixes(self):
        target = {"name": "guest_target", "artifacts": [{"path": "tests/guest.check"}]}
        aliases = build_changed_guest_tests.target_artifact_names(target)
        self.assertEqual(build_changed_guest_tests.test_owner(["/build/tests/guest.check"], aliases), "guest_target")
        self.assertIsNone(build_changed_guest_tests.test_owner(["/build/tests/guest.other"], aliases))
        self.assertIsNone(build_changed_guest_tests.test_owner(["/build/tests/guest.check"], {"guest": "unrelated"}))

    def test_dotted_windows_artifact_names_remove_only_exe(self):
        target = {"name": "guest_target", "artifacts": [{"path": "tests/guest.check.EXE"}]}
        aliases = build_changed_guest_tests.target_artifact_names(target)
        self.assertEqual(build_changed_guest_tests.test_owner(["C:/build/tests/guest.check.EXE"], aliases), "guest_target")
        self.assertEqual(build_changed_guest_tests.test_owner(["python", "C:/build/tests/guest.check.EXE"], aliases), "guest_target")


if __name__ == "__main__":
    with contextlib.redirect_stdout(io.StringIO()):
        unittest.main()
