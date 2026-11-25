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


def runs_selected_tests(command):
    return "-I" in command and "--show-only=json-v1" not in command


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

    def test_tests_in_checkout_path_do_not_make_a_production_tool_a_guest_test(self):
        root = Path(self.temporary.name).resolve() / "tests" / "AnyPS5"
        guest = self.target("guest_clock_tests", fragment="libc.prx")
        helper = self.target("helper", source="core/libs/prx/libc/Helper.cpp", fragment="libc.prx")
        targets = {target["id"]: target for target in (guest, helper)}
        self.assertEqual(build_changed_guest_tests.guest_test_targets(root, targets), ["guest_clock_tests"])

    def test_a_production_source_change_selects_no_test_but_reaches_the_run(self):
        guest = self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")
        self.assertEqual(self.selections(self.fixture([guest]), ["core/libs/prx/libc/src/FloatBuiltins.cpp"]), [])

    def test_changed_static_fixture_selects_its_guest_consumer(self):
        guest = self.target("guest_jxr_tests", source="core/libs/tests/GuestJxr.cpp", fragment="libc.prx")
        unrelated = self.target("guest_pad_tests", fragment="libc.prx")
        fixture_source = "core/libs/tests/JxrTileInfoFixture.c"
        fixture = self.target("jxr_tile_info_fixture", target_type="STATIC_LIBRARY", source=fixture_source)
        guest["dependencies"] = [{"id": fixture["id"]}]
        build = self.fixture([guest, unrelated, fixture])
        self.assertEqual(self.selections(build, [fixture_source]), ["guest_jxr_tests"])

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
        for path in ("3rdparty/ZArchive", "cmake/Opus.cmake", ".gitmodules"):
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


class PhaseReport(unittest.TestCase):
    def report(self, label, seconds, detail=""):
        with mock.patch.object(build_changed_guest_tests.time, "perf_counter", return_value=10.0 + seconds):
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                elapsed = build_changed_guest_tests.phase_report(label, 10.0, detail)
        return output.getvalue(), elapsed

    def test_prints_the_elapsed_seconds_with_one_decimal(self):
        self.assertEqual(self.report("build libs", 2.5), ("[ci-timing] build libs: 2.5 s\n", 2.5))

    def test_appends_the_detail_in_parentheses(self):
        self.assertEqual(
            self.report("build selected test targets", 123.4, "12 targets in 2 batches"),
            ("[ci-timing] build selected test targets: 123.4 s (12 targets in 2 batches)\n", 123.4),
        )


class CompilerCacheReport(unittest.TestCase):
    def test_runs_ccache_show_stats(self):
        with mock.patch.object(build_changed_guest_tests.subprocess, "run") as run:
            build_changed_guest_tests.report_compiler_cache()
        run.assert_called_once_with(["ccache", "--show-stats"], check=False)

    def test_a_missing_ccache_is_ignored(self):
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=FileNotFoundError):
            build_changed_guest_tests.report_compiler_cache()


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
            if runs_selected_tests(cmd):
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
            if runs_selected_tests(cmd):
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
            if runs_selected_tests(cmd):
                selections.append(Path(cmd[cmd.index("-I") + 1]).read_text())
            return run_result(listing if "--show-only=json-v1" in cmd else "")
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute):
            self.assertEqual(self.main(build, ["core/libs/prx/libSceXml/include/Xml.hpp"]), 0)
        self.assertEqual(selections, ["0,0,0,1,2,3,4\n"])

    def test_a_changed_list_with_a_bom_keeps_the_first_path(self):
        build = self.fixture([self.target("guest_pad_tests", fragment="libc.prx")])
        listing = json.dumps({"tests": [
            {"name": "guest_pad", "command": ["C:/b/tests/guest_pad_tests.exe"]},
            {"name": "public_headers", "command": ["C:/python.exe", "test_public_headers.py"]},
        ]})
        changed = Path(self.temporary.name) / "changed.txt"
        changed.write_bytes(b"\xef\xbb\xbfcore/libs/include/Changed.hpp\r\n")
        selections = []

        def execute(cmd, **_):
            if runs_selected_tests(cmd):
                selections.append(Path(cmd[cmd.index("-I") + 1]).read_text())
            return run_result(listing if "--show-only=json-v1" in cmd else "")

        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute):
            self.assertEqual(build_changed_guest_tests.main([str(build), str(changed)]), 0)
        self.assertEqual(selections, ["0,0,0,1,2\n"])

    def test_a_production_source_change_does_not_run_the_public_header_tests(self):
        build = self.fixture([self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")])
        listing = json.dumps({"tests": [{"name": "public_headers", "command": ["C:/python.exe", "x.py"]}]})
        selections = []
        def execute(cmd, **_):
            if runs_selected_tests(cmd):
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

    def test_a_nid_change_runs_the_nid_script_tests(self):
        build = self.fixture([self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")])
        listing = json.dumps({"tests": [
            {"name": "guest_pad", "command": ["C:/b/tests/guest_pad_tests.exe"]},
            {"name": "nid_gnu_hash", "command": ["C:/python.exe", "test_gnu_hash.py"]},
            {"name": "nid_patcher_native_paths", "command": ["C:/python.exe", "test_native_paths.py"]},
            {"name": "nidx_not_a_nid_test", "command": ["C:/python.exe", "x.py"]},
        ]})
        selections = []
        def execute(cmd, **_):
            if runs_selected_tests(cmd):
                selections.append(Path(cmd[cmd.index("-I") + 1]).read_text())
            return run_result(listing if "--show-only=json-v1" in cmd else "")
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute):
            self.assertEqual(self.main(build, ["core/libs/nid/src/main.cpp"]), 0)
            self.assertEqual(self.main(build, ["core/libs/nid/tests/test_native_paths.py"]), 0)
        self.assertEqual(selections, ["0,0,0,2,3\n", "0,0,0,2,3\n"])

    def test_a_nid_change_builds_the_executable_behind_a_nid_test(self):
        build = self.fixture([
            self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx"),
            self.target("nid_buffer_validation_tests", source="core/libs/nid/tests/BufferValidation.cpp"),
        ])
        listing = json.dumps({"tests": [
            {"name": "guest_pad", "command": ["C:/b/tests/guest_pad_tests.exe"]},
            {"name": "nid_buffer_validation", "command": []},
            {"name": "nid_gnu_hash", "command": ["C:/python.exe", "test_gnu_hash.py"]},
        ]})
        testfile = build / "core" / "libs" / "CTestTestfile.cmake"
        testfile.parent.mkdir(parents=True)
        (build / "CTestTestfile.cmake").write_text('subdirs("core/libs")\n', encoding="utf-8")
        testfile.write_text('add_test("guest_pad" "C:/b/tests/guest_pad_tests.exe")\n'
                            'add_test("nid_buffer_validation" "C:/b/tests/nid_buffer_validation_tests.exe")\n'
                            'set_tests_properties("nid_buffer_validation" PROPERTIES TIMEOUT "10")\n'
                            'add_test("nid_gnu_hash" "C:/python.exe" "test_gnu_hash.py")\n', encoding="utf-8")
        commands = []
        def execute(cmd, **_):
            commands.append(list(cmd))
            return run_result(listing if "--show-only=json-v1" in cmd else "")
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute):
            self.assertEqual(self.main(build, ["core/libs/nid/src/main.cpp"]), 0)
        built = [target for cmd in commands if "--target" in cmd for target in cmd[cmd.index("--target") + 1:]]
        self.assertIn("nid_buffer_validation_tests", built)
        ran = [index for index, cmd in enumerate(commands) if runs_selected_tests(cmd)]
        builds = [index for index, cmd in enumerate(commands) if "--target" in cmd and "nid_buffer_validation_tests" in cmd]
        self.assertTrue(ran and builds and max(builds) < min(ran))

    def test_registered_commands_ignore_stale_testfiles(self):
        build = Path(self.temporary.name) / "tree"
        (build / "sub").mkdir(parents=True)
        (build / "stale").mkdir()
        (build / "CTestTestfile.cmake").write_text(
            'add_test("a b" "/x/a_tests" "arg \\"q\\"")\nsubdirs("sub")\n', encoding="utf-8")
        (build / "sub" / "CTestTestfile.cmake").write_text(
            '  add_test([=[nid_one]=] "/x/nid_one_tests")\nset_tests_properties("nid_one" PROPERTIES TIMEOUT "5")\n', encoding="utf-8")
        (build / "stale" / "CTestTestfile.cmake").write_text(
            'add_test("nid_one" "/old/nid_one_tests")\nadd_test("removed" "/old/removed_tests")\n', encoding="utf-8")
        self.assertEqual(build_changed_guest_tests.registered_test_commands(build), {
            "a b": ["/x/a_tests", 'arg "q"'], "nid_one": ["/x/nid_one_tests"]})

    def test_registered_commands_preserve_empty_arguments(self):
        build = Path(self.temporary.name)
        (build / "CTestTestfile.cmake").write_text(
            'add_test([=[fixture]=] "/x/fixture" "" [=[]=] tail "")\n', encoding="utf-8")
        self.assertEqual(build_changed_guest_tests.registered_test_commands(build),
                         {"fixture": ["/x/fixture", "", "", "tail", ""]})

    def test_a_change_outside_nid_does_not_run_the_nid_script_tests(self):
        build = self.fixture([self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")])
        listing = json.dumps({"tests": [{"name": "nid_gnu_hash", "command": ["C:/python.exe", "x.py"]}]})
        selections = []
        def execute(cmd, **_):
            if runs_selected_tests(cmd):
                selections.append(Path(cmd[cmd.index("-I") + 1]).read_text())
            return run_result(listing if "--show-only=json-v1" in cmd else "")
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute):
            self.assertEqual(self.main(build, ["core/libs/prx/libSceXml/src/Xml.cpp"]), 0)
        self.assertEqual(selections, [])

    def test_a_missing_nid_registration_is_an_error(self):
        build = self.fixture([self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")])
        listing = json.dumps({"tests": [{"name": "guest_pad", "command": ["C:/b/tests/guest_pad_tests.exe"]}]})
        def execute(cmd, **_):
            return run_result(listing if "--show-only=json-v1" in cmd else "")
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(self.main(build, ["core/libs/nid/src/main.cpp"]), 1)

    def test_nid_inputs(self):
        for path in ("core/libs/nid/src/main.cpp", "core/libs/nid/tests/test_gnu_hash.py", "core/libs/CMakeLists.txt", "3rdparty/ZArchive"):
            with self.subTest(path=path):
                self.assertTrue(build_changed_guest_tests.checks_nid([path]))
        for path in ("core/libs/prx/libSceXml/src/Xml.cpp", "core/libs/tests/Guest.cpp", "core/relinker/R.cpp", "README.md"):
            with self.subTest(path=path):
                self.assertFalse(build_changed_guest_tests.checks_nid([path]))

    def test_script_test_indices_are_one_based_sorted_and_unmatched_pattern_raises(self):
        listing = json.dumps({"tests": [{"name": "nid_b"}, {"name": "guest"}, {"name": "public_headers"}, {"name": "nid_a"}]})
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", return_value=run_result(listing)) as run:
            self.assertEqual(build_changed_guest_tests.script_test_indices(
                "build", [build_changed_guest_tests.NID_TESTS, build_changed_guest_tests.PUBLIC_HEADER_TESTS]), [1, 3, 4])
            with self.assertRaises(ValueError):
                build_changed_guest_tests.script_test_indices("build", [build_changed_guest_tests.re.compile("^absent$")])
        self.assertEqual(run.call_args.kwargs["encoding"], "utf-8")

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
        run_cmd = [cmd for cmd in calls if runs_selected_tests(cmd)]
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
            if runs_selected_tests(cmd):
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
        self.assertEqual(len([cmd for cmd in calls if runs_selected_tests(cmd)]), 1)

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

    @unittest.skipUnless(shutil.which("cmake") and shutil.which("ctest"), "CMake and CTest are required")
    def test_changed_fixture_setup_runs_its_consumer(self):
        build = Path(self.temporary.name)
        python = Path(sys.executable).as_posix()
        cmake = Path(shutil.which("cmake")).as_posix()
        (build / "CTestTestfile.cmake").write_text(
            f'add_test(setup "{python}" -c "open(\'fixture\', \'w\').close()")\n'
            'set_tests_properties(setup PROPERTIES FIXTURES_SETUP artifact)\n'
            f'add_test(consumer "{cmake}" -E false)\n'
            'set_tests_properties(consumer PROPERTIES FIXTURES_REQUIRED artifact)\n'
            f'add_test(unrelated "{cmake}" -E touch unrelated)\n', encoding="utf-8")
        real_run = subprocess.run

        def execute(cmd, **kwargs):
            if cmd[0] == "cmake":
                return run_result()
            return real_run(cmd, **{**kwargs, "capture_output": True, "text": True})

        target = {"name": "fixture_target", "artifacts": [{"path": python}]}
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute):
            with self.assertRaises(subprocess.CalledProcessError) as failure:
                build_changed_guest_tests.run(build, ["fixture_target"],
                                              build_changed_guest_tests.target_artifact_names(target))
        self.assertIn("consumer", failure.exception.stdout)
        self.assertTrue((build / "fixture").exists())
        self.assertFalse((build / "unrelated").exists())

    @unittest.skipUnless(shutil.which("cmake") and shutil.which("ctest") and shutil.which("ninja")
                         and (shutil.which("gcc") or shutil.which("cc")), "requires CMake, CTest, Ninja and a C compiler")
    def test_fresh_fixture_schedule_builds_required_setup_and_cleanup(self):
        source = Path(self.temporary.name)
        build = source / "build"
        (source / "CMakeLists.txt").write_text(
            """cmake_minimum_required(VERSION 3.20)
project(FixtureSchedule LANGUAGES C)
enable_testing()
add_custom_target(libs)
foreach(target producer aux_producer validator verifier cleaner unrelated)
    add_executable(${target} EXCLUDE_FROM_ALL marker.c)
endforeach()
add_test(NAME produce COMMAND producer artifact)
add_test(NAME prepare_aux COMMAND aux_producer auxiliary)
add_test(NAME validate COMMAND validator validated artifact auxiliary)
add_test(NAME verify COMMAND verifier verified validated)
add_test(NAME clean COMMAND cleaner cleaned verified)
add_test(NAME unrelated COMMAND unrelated unrelated)
set_tests_properties(produce PROPERTIES FIXTURES_SETUP artifact)
set_tests_properties(prepare_aux PROPERTIES FIXTURES_SETUP auxiliary)
set_tests_properties(validate PROPERTIES FIXTURES_REQUIRED "artifact;auxiliary" FIXTURES_SETUP validated)
set_tests_properties(verify PROPERTIES FIXTURES_REQUIRED "artifact;validated")
set_tests_properties(clean PROPERTIES FIXTURES_CLEANUP artifact)
""", encoding="utf-8")
        (source / "marker.c").write_text(
            """#include <stdio.h>
int main(int argc, char **argv) {
    for (int i = 2; i < argc; ++i) {
        FILE *input = fopen(argv[i], "r");
        if (!input) return 1;
        fclose(input);
    }
    FILE *marker = fopen(argv[1], "w");
    if (!marker) return 1;
    return fclose(marker);
}
""", encoding="utf-8")
        compiler = shutil.which("gcc") or shutil.which("cc")
        subprocess.run(["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
                        f"-DCMAKE_C_COMPILER={compiler}"], check=True, capture_output=True)
        targets = {name: name for name in ("producer", "aux_producer", "validator", "verifier", "cleaner", "unrelated")}
        build_changed_guest_tests.run(build, ["producer"], targets)
        for marker in ("artifact", "auxiliary", "validated", "verified", "cleaned"):
            self.assertTrue((build / marker).is_file(), marker)
        self.assertFalse((build / "unrelated").exists())

    @unittest.skipUnless(shutil.which("ctest"), "CTest is required to run the selected tests")
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

    def test_changed_shared_test_library_selects_registered_commands_and_builds_their_inputs(self):
        source = "core/libs/tests/SharedFixture.cpp"
        fixture = self.target("fixture", "SHARED_LIBRARY", source)
        fixture["artifacts"] = [{"path": "modules/fixture.prx"}]
        companion = self.target("companion", "MODULE_LIBRARY", "core/libs/tests/Companion.cpp")
        companion["artifacts"] = [{"path": "modules/companion.prx"}]
        runner = self.target("runner", source="core/relinker/Runner.cpp")
        build = self.fixture([fixture, companion, runner])
        program = (build / "tests/runner.exe").as_posix()
        library = (build / "modules/fixture.prx").as_posix()
        other = (build / "elsewhere/fixture.prx").as_posix()
        dependency = (build / "modules/companion.prx").as_posix()
        names = ["argument.one", "argument[2]", "unrelated"]
        (build / "CTestTestfile.cmake").write_text(
            f'add_test("argument.one" "{program}" "{library}" "{dependency}")\n'
            f'add_test("argument[2]" "{program}" "-DLIB={library}")\n'
            f'add_test("unrelated" "{program}" "{other}")\n', encoding="utf-8")
        builds, selections = [], []

        def execute(command, **kwargs):
            if "--target" in command:
                builds.append(command[command.index("--target") + 1:])
            elif "--show-only=json-v1" in command:
                scheduled = names[:2] if "-I" in command else names
                return run_result(json.dumps({"tests": [{"name": name} for name in scheduled]}))
            elif runs_selected_tests(command):
                selections.append(Path(command[command.index("-I") + 1]).read_text())
            return run_result()

        output = io.StringIO()
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute), contextlib.redirect_stdout(output):
            self.assertEqual(self.main(build, [source]), 0)
        self.assertEqual(selections, ["0,0,0,1,2\n"])
        self.assertEqual(builds, [["libs"], ["companion", "fixture", "runner"]])
        self.assertIn("(3 targets, 2 tests)", output.getvalue())

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
            if runs_selected_tests(cmd):
                selections.append(Path(cmd[cmd.index("-I") + 1]).read_text())
            return run_result(listing if "--show-only=json-v1" in cmd else "")
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute):
            code = self.main(build, ["core/libs/tests/Guest.cpp"])
        self.assertEqual(code, 0)
        commands = [cmd for cmd in calls if cmd[0] == "ctest" and "--show-only=json-v1" not in cmd]
        self.assertEqual(len(commands), 1)
        self.assertEqual(selections, ["0,0,0,1\n"])

    def test_reports_every_phase_even_when_ctest_fails(self):
        build = self.fixture([self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")])
        listing = json.dumps({"tests": [{"name": "guest_pad", "command": ["C:/b/tests/guest_pad_tests.exe"]}]})
        output = io.StringIO()
        def execute(cmd, **_):
            if cmd[0] == "ctest" and "--show-only=json-v1" not in cmd:
                raise subprocess.CalledProcessError(8, cmd)
            return run_result(listing if "--show-only=json-v1" in cmd else "")
        with mock.patch.object(build_changed_guest_tests.subprocess, "run", side_effect=execute), \
                contextlib.redirect_stdout(output):
            code = self.main(build, ["core/libs/tests/Guest.cpp"])
        self.assertEqual(code, 1)
        reported = output.getvalue()
        self.assertIn("[ci-timing] build libs:", reported)
        self.assertIn("[ci-timing] build selected test targets: ", reported)
        self.assertIn("[ci-timing] ctest discovery: ", reported)
        self.assertIn("[ci-timing] ctest: ", reported)

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


class ChangeClassification(unittest.TestCase):
    def test_reaches_libs_only_for_library_and_build_inputs(self):
        for path in ("CMakeLists.txt", "core/libs/a.cpp", ".github/workflows/build.yml", "tools/x.py"):
            self.assertTrue(build_changed_guest_tests.reaches_libs([path]), path)
        self.assertFalse(build_changed_guest_tests.reaches_libs(["docs/readme.md", "core/other/a.cpp"]))
        self.assertFalse(build_changed_guest_tests.reaches_libs([]))

    def test_script_test_patterns_follow_the_changed_inputs(self):
        patterns = build_changed_guest_tests.script_test_patterns
        self.assertEqual(patterns(["docs/readme.md"]), [])
        self.assertEqual(patterns(["core/libs/nid/a.cpp"]), [build_changed_guest_tests.NID_TESTS])
        self.assertEqual(
            patterns(["CMakeLists.txt"]),
            [build_changed_guest_tests.PUBLIC_HEADER_TESTS, build_changed_guest_tests.NID_TESTS])

    def test_test_owner_of_an_empty_command_is_none(self):
        self.assertIsNone(build_changed_guest_tests.test_owner([], {"guest": "guest"}))

class CoreLibTestTargetsTests(unittest.TestCase):
    def test_guest_and_host_only_targets_are_merged_sorted_and_deduplicated(self):
        root, targets, changed = Path("root"), {"t": {}}, ["core/libs/x.cpp"]
        with mock.patch.object(build_changed_guest_tests, "guest_test_targets", return_value=["beta", "Alpha", "shared"]) as guest, \
                mock.patch.object(build_changed_guest_tests.host_only, "host_only_test_targets", return_value=["shared", "alpha2"]) as host:
            result = build_changed_guest_tests.core_lib_test_targets(root, targets, changed)
        self.assertEqual(result, ["Alpha", "alpha2", "beta", "shared"])
        guest.assert_called_once_with(root, targets, changed)
        host.assert_called_once_with(root, targets, changed)

    def test_changed_paths_default_to_none(self):
        with mock.patch.object(build_changed_guest_tests, "guest_test_targets", return_value=[]) as guest, \
                mock.patch.object(build_changed_guest_tests.host_only, "host_only_test_targets", return_value=[]) as host:
            self.assertEqual(build_changed_guest_tests.core_lib_test_targets(Path("root"), {}), [])
        guest.assert_called_once_with(Path("root"), {}, None)
        host.assert_called_once_with(Path("root"), {}, None)


class ScheduledTestOwnersTests(unittest.TestCase):
    TARGETS = {"alpha": "Alpha", "beta": "beta"}

    def owners(self, tests, registered):
        with mock.patch.object(build_changed_guest_tests.subprocess, "run") as run, \
                mock.patch.object(build_changed_guest_tests, "registered_test_commands", return_value=registered):
            run.return_value.stdout = json.dumps({"tests": tests})
            owners = build_changed_guest_tests.scheduled_test_owners(Path("build"), "indices.txt", self.TARGETS)
        self.assertEqual(run.call_args.args[0], ["ctest", "--test-dir", "build", "--show-only=json-v1", "-I", "indices.txt"])
        return owners

    def test_scheduled_tests_contribute_owners(self):
        tests = [
            {"name": "py_beta", "command": ["python", "bin/beta.exe"]},
            {"name": "py_alpha", "command": ["python", "bin/alpha.exe"]},
        ]
        self.assertEqual(self.owners(tests, {}), {"Alpha", "beta"})

    def test_registered_commands_are_used_when_the_listed_command_has_no_owner(self):
        tests = [{"name": "py_beta", "command": ["python", "script.py"]}, {"name": "unbuilt"}]
        tool = (Path.cwd() / "bin/alpha.exe").as_posix()
        registered = {"py_beta": ["python", "bin/beta.exe"], "unbuilt": ["cmake", f"-DTOOL={tool}", "-P", "verify.cmake"]}
        self.assertEqual(self.owners(tests, registered), {"Alpha", "beta"})
        self.assertEqual(self.owners(tests, {}), set())

    def test_owner_is_reported_once(self):
        tests = [{"name": "py_a", "command": ["bin/alpha.exe"]}, {"name": "py_b", "command": ["bin/alpha.exe", "--x"]}]
        self.assertEqual(self.owners(tests, {}), {"Alpha"})


class CtestResolutionsTests(unittest.TestCase):
    TARGETS = {"alpha": "alpha", "beta": "beta"}

    def resolve(self, tests, **kwargs):
        with mock.patch.object(build_changed_guest_tests.subprocess, "run") as run:
            run.return_value.stdout = json.dumps({"tests": tests})
            result = build_changed_guest_tests.ctest_resolutions(Path("build"), self.TARGETS, **kwargs)
        self.assertEqual(run.call_args.args[0], ["ctest", "--test-dir", "build", "--show-only=json-v1"])
        return result

    def ctest_entries(self):
        return [
            {"name": "a1", "command": ["bin/alpha.exe"]},
            {"name": "b1", "command": ["cmake", "-P", "x.cmake", "--", "bin/beta.exe"]},
            {"name": "unknown", "command": ["bin/other.exe"]},
            {"name": "c1", "command": ["bin/alpha.exe", "bin/beta.exe"]},
        ]

    def test_every_test_with_a_known_owner_is_resolved_with_its_position(self):
        self.assertEqual(self.resolve(self.ctest_entries()), {"a1": ("alpha", 1), "b1": ("beta", 2), "c1": ("alpha", 4)})

    def test_cmake_env_preserves_the_owner_before_an_application_separator(self):
        tests = [
            {"name": "env_application_separator", "command": [
                "C:/Tools/cmake.exe", "-E", "env", "--modify", "MODE=set:bin/beta.exe",
                "bin/alpha.exe", "--", "bin/beta.exe"]},
            {"name": "env_wrapper_separator", "command": [
                "C:/Tools/cmake.exe", "-E", "env", "--unset=MODE", "--", "bin/beta.exe", "--", "sample"]},
        ]
        self.assertEqual(self.resolve(tests), {"env_application_separator": ("alpha", 1), "env_wrapper_separator": ("beta", 2)})

    def test_selection_matches_the_owner_or_a_referenced_target(self):
        self.assertEqual(self.resolve(self.ctest_entries(), selected={"beta"}), {"b1": ("beta", 2), "c1": ("alpha", 4)})
        self.assertEqual(self.resolve(self.ctest_entries(), selected=set()), {})

    def test_fixture_consumers_can_set_up_fixtures(self):
        tests = [
            {"name": "first", "command": ["bin/alpha.exe"],
             "properties": [{"name": "FIXTURES_SETUP", "value": ["first_artifact"]}]},
            {"name": "second", "properties": [{"name": "FIXTURES_REQUIRED", "value": ["first_artifact"]},
                                               {"name": "FIXTURES_SETUP", "value": ["second_artifact"]}]},
            {"name": "third", "command": ["cmake", "-P", "verify.cmake"],
             "properties": [{"name": "FIXTURES_REQUIRED", "value": ["second_artifact"]}]},
            {"name": "unrelated", "command": ["cmake", "-P", "verify.cmake"]},
        ]
        self.assertEqual(self.resolve(tests, selected={"alpha"}),
                         {"first": ("alpha", 1), "second": (None, 2), "third": (None, 3)})

    def test_document_without_tests_resolves_nothing(self):
        self.assertEqual(self.resolve([]), {})


if __name__ == "__main__":
    with contextlib.redirect_stdout(io.StringIO()):
        unittest.main()
