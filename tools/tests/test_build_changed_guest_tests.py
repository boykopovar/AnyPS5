import contextlib
import io
import json
import re
from pathlib import Path
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

    def test_an_unrelated_change_selects_nothing(self):
        guest = self.target("guest_pad_tests", source="core/libs/tests/Guest.cpp", fragment="libc.prx")
        self.assertEqual(self.selections(self.fixture([guest]), ["core/relinker/codegen/src/Amd64OnlyConverter.cpp"]), [])


class OwnerResolution(unittest.TestCase):
    def test_resolves_the_executable_not_a_test_argument(self):
        owner = build_changed_guest_tests.test_owner(
            ["C:/b/tests/guest_np_common_tests.exe", "build"], {"guest_np_common_tests": "guest_np_common_tests"})
        self.assertEqual(owner, "guest_np_common_tests")

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
        with mock.patch.object(build_changed_guest_tests.subprocess, "run",
                               side_effect=lambda cmd, **_: calls.append(cmd) or run_result(self.listing() if cmd[:3] == ["ctest", "--test-dir", str(build)] and "--show-only=json-v1" in cmd else "")):
            code = self.main(build, ["core/libs/tests/GuestNp.cpp", "core/libs/tests/Guest.cpp"])
        self.assertEqual(code, 0)
        run_cmd = [cmd for cmd in calls if cmd[0] == "ctest" and not any("--show-only" in part for part in cmd)][0]
        pattern = run_cmd[run_cmd.index("-R") + 1]
        self.assertIn(re.escape("guest_np_common_build"), pattern)
        self.assertIn(re.escape("guest_np_common_fetch"), pattern)
        self.assertIn(re.escape("guest_pad"), pattern)
        self.assertIn("--no-tests=error", run_cmd)

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
        with mock.patch.object(build_changed_guest_tests.subprocess, "run",
                               side_effect=lambda cmd, **_: calls.append(cmd) or run_result(listing if "--show-only=json-v1" in cmd else "")):
            code = self.main(build, ["core/libs/tests/Guest.cpp"])
        self.assertEqual(code, 0)
        commands = [cmd for cmd in calls if cmd[0] == "ctest" and "--show-only=json-v1" not in cmd]
        self.assertEqual(len(commands), 1)
        self.assertEqual(commands[0][commands[0].index("-R") + 1], "^(guest_pad)$")

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


if __name__ == "__main__":
    with contextlib.redirect_stdout(io.StringIO()):
        unittest.main()
