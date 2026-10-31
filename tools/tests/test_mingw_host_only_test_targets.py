import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_mingw_host_only_tests


class HostOnlyTargets(unittest.TestCase):
    def target(self, name, target_type="EXECUTABLE", source="core/libs/tests/HostOnly.cpp", dependencies=(), fragment=""):
        return {
            "artifacts": [{"path": f"tests/{name}.exe"}],
            "dependencies": [{"id": dependency} for dependency in dependencies],
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

    def test_selects_core_libs_test_executables_without_guest_libc(self):
        host = self.target("host_tests")
        guest = self.target("guest_tests", dependencies=("libc-id",),
                             fragment="C:/build/core/libs/prx/libc/libc.dll.a")
        libc = self.target("libc", target_type="SHARED_LIBRARY")
        libc["id"] = "libc-id"
        other = self.target("outside_tests", source="core/relinker/tests/Outside.cpp")
        utility = self.target("not_a_test", source="core/libs/nid/src/main.cpp")
        build_dir = self.fixture([host, guest, libc, other, utility])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(build_mingw_host_only_tests.host_only_test_targets(source_root, targets), ["host_tests"])

    def test_excludes_a_transitive_guest_libc_dependency(self):
        helper = self.target("guest_helper", target_type="STATIC_LIBRARY", source="core/libs/tests/GuestHelper.cpp",
                             dependencies=("libc-id",))
        guest = self.target("guest_tests", dependencies=("helper-id",),
                             fragment="C:/build/core/libs/prx/libc/libc.dll.a")
        libc = self.target("libc", target_type="SHARED_LIBRARY")
        helper["id"] = "helper-id"
        libc["id"] = "libc-id"
        build_dir = self.fixture([guest, helper, libc])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(build_mingw_host_only_tests.host_only_test_targets(source_root, targets), [])

    def test_excludes_a_direct_libc_link_but_ignores_the_full_libraries_build_target(self):
        direct_file = self.target("direct_file_tests", fragment="/build/libs/libc.prx")
        all_libraries = self.target("all_libraries_tests", dependencies=("libs-id",))
        libraries = self.target("libs", target_type="UTILITY")
        libraries["id"] = "libs-id"
        build_dir = self.fixture([direct_file, all_libraries, libraries])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(build_mingw_host_only_tests.host_only_test_targets(source_root, targets), ["all_libraries_tests"])

    def test_excludes_product_library_dependencies_but_keeps_test_helpers(self):
        product = self.target("product_lib", target_type="STATIC_LIBRARY", source="core/libs/utility/src/Utility.cpp")
        product["artifacts"] = [{"path": "core/libs/utility/libproduct_lib.a"}]
        product["id"] = "product-id"
        production_dependent = self.target("production_dependent", dependencies=("product-id",),
                                            fragment="/build/core/libs/utility/libproduct_lib.a")
        helper = self.target("test_helper", target_type="STATIC_LIBRARY", source="core/libs/tests/GuestHelper.cpp")
        helper["paths"]["source"] = "core/libs/tests"
        helper["id"] = "helper-id"
        helper_dependent = self.target("helper_dependent", dependencies=("helper-id",), source="core/libs/tests/HostOnly.cpp")
        external = self.target("external", target_type="STATIC_LIBRARY", source="3rdparty/sdl/src/SDL.cpp")
        external["paths"]["source"] = "3rdparty/sdl/src"
        external["id"] = "external-id"
        external_dependent = self.target("external_dependent", dependencies=("external-id",))
        build_dir = self.fixture([product, production_dependent, helper, helper_dependent, external, external_dependent])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(build_mingw_host_only_tests.host_only_test_targets(source_root, targets), ["external_dependent", "helper_dependent"])

    def test_ignores_build_order_dependencies_that_are_not_linked(self):
        host = self.target(
            "guest_sigvec_modules_tests",
            dependencies=("libc-id", "kernel-id", "internal-id", "patcher-id"),
        )
        libc = self.target("libc", target_type="SHARED_LIBRARY", source="core/libs/libc/src/libc.cpp")
        kernel = self.target("libkernel", target_type="SHARED_LIBRARY", source="core/libs/libkernel/src/kernel.cpp")
        internal = self.target("libSceLibcInternal", target_type="SHARED_LIBRARY", source="core/libs/internal/src/internal.cpp")
        patcher = self.target("nid_patcher", target_type="STATIC_LIBRARY", source="core/libs/nid_patcher/src/patcher.cpp")
        libc["id"], kernel["id"], internal["id"], patcher["id"] = "libc-id", "kernel-id", "internal-id", "patcher-id"
        build_dir = self.fixture([host, libc, kernel, internal, patcher])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(
            build_mingw_host_only_tests.host_only_test_targets(source_root, targets),
            ["guest_sigvec_modules_tests"],
        )

    def test_selects_host_tests_when_transitive_dependency_source_changes(self):
        host_a = self.target("host_a", dependencies=("static_helper",))
        host_b = self.target("host_b", dependencies=("static_helper",))
        unrelated = self.target("unrelated", dependencies=("unrelated_helper",), source="core/libs/tests/Unrelated.cpp")
        static_helper = self.target(
            "static_helper",
            target_type="STATIC_LIBRARY",
            source="core/libs/tests/StaticHelper.cpp",
            dependencies=("object_helper",),
        )
        static_helper["paths"]["source"] = "core/libs/tests"
        helper_order = self.target(
            "helper_order",
            target_type="UTILITY",
            source="core/libs/tests/BuildInput.txt",
            dependencies=("object_helper",),
        )
        object_helper = self.target(
            "object_helper",
            target_type="OBJECT_LIBRARY",
            source="core/libs/src/Helper.cpp",
        )
        unrelated_helper = self.target(
            "unrelated_helper",
            target_type="STATIC_LIBRARY",
            source="core/libs/tests/UnrelatedHelper.cpp",
        )
        unrelated_helper["paths"]["source"] = "core/libs/tests"
        build_dir = self.fixture([host_a, host_b, unrelated, static_helper, helper_order, object_helper, unrelated_helper])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(
            build_mingw_host_only_tests.host_only_test_targets(
                source_root, targets, ["core/libs/src/Helper.cpp"]
            ),
            ["host_a", "host_b"],
        )
        self.assertEqual(
            build_mingw_host_only_tests.host_only_test_targets(
                source_root, targets, ["core/libs/tests/StaticHelper.cpp"]
            ),
            ["host_a", "host_b"],
        )
        self.assertEqual(
            build_mingw_host_only_tests.host_only_test_targets(
                source_root, targets, ["core/libs/tests/UnrelatedHelper.cpp"]
            ),
            ["unrelated"],
        )

    def test_ignores_order_only_product_and_utility_sources(self):
        for kind in ("UTILITY", "SHARED_LIBRARY", "MODULE_LIBRARY"):
            with self.subTest(kind=kind):
                host = self.target("host_tests", dependencies=("order",))
                order = self.target("order", target_type=kind, source="core/libs/prx/libkernel/Source.cpp",
                                    dependencies=("hidden",))
                hidden = self.target("hidden", target_type="OBJECT_LIBRARY", source="core/libs/src/Hidden.cpp")
                root = Path(self.temporary.name).resolve()
                targets = {item["id"]: item for item in (host, order, hidden)}
                for changed in ("core/libs/prx/libkernel/Source.cpp", "core/libs/src/Hidden.cpp"):
                    self.assertEqual(build_mingw_host_only_tests.host_only_test_targets(root, targets, [changed]), [])
                self.assertEqual(build_mingw_host_only_tests.host_only_test_targets(
                    root, targets, ["core/libs/tests/HostOnly.cpp"]), ["host_tests"])

    def test_interface_sources_are_reached_and_invalid_source_paths_are_controlled(self):
        host = self.target("host_tests", dependencies=("helper",))
        helper = self.target("helper", target_type="INTERFACE_LIBRARY", source="core/libs/tests/Interface.cpp")
        helper["paths"]["source"] = "core/libs/tests"
        root = Path(self.temporary.name).resolve()
        targets = {item["id"]: item for item in (host, helper)}
        self.assertEqual(build_mingw_host_only_tests.host_only_test_targets(
            root, targets, ["core/libs/tests/Interface.cpp"]), ["host_tests"])
        for entry in ({}, {"path": None}, {"path": []}, {"path": ""}, None):
            with self.subTest(entry=entry):
                helper["sources"] = [entry]
                with self.assertRaisesRegex(ValueError, "source"):
                    build_mingw_host_only_tests.host_only_test_targets(root, targets, ["core/libs/tests/Interface.cpp"])

    def test_dependency_cycle_terminates(self):
        host = self.target("cyclic_tests", dependencies=("cycle_a",))
        cycle_a = self.target(
            "cycle_a",
            target_type="STATIC_LIBRARY",
            source="core/libs/tests/CycleA.cpp",
            dependencies=("cycle_b",),
        )
        cycle_a["paths"]["source"] = "core/libs/tests"
        cycle_b = self.target(
            "cycle_b",
            target_type="OBJECT_LIBRARY",
            source="core/libs/src/CycleB.cpp",
            dependencies=("cycle_a",),
        )
        build_dir = self.fixture([host, cycle_a, cycle_b])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(
            build_mingw_host_only_tests.host_only_test_targets(
                source_root, targets, ["core/libs/src/CycleB.cpp"]
            ),
            ["cyclic_tests"],
        )

    def test_fails_when_target_dependency_is_missing(self):
        host = self.target("host_tests", dependencies=("missing_id",))
        build_dir = self.fixture([host])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        with self.assertRaisesRegex(ValueError, "missing_id"):
            build_mingw_host_only_tests.host_only_test_targets(
                source_root, targets, ["core/libs/tests/HostOnly.cpp"]
            )

    def test_invalid_dependency_entries_raise_controlled_errors(self):
        source_root = Path(self.temporary.name).resolve()
        for dependencies in (None, "helper", [None], ["helper"], [{"id": ["helper"]}], [{}]):
            with self.subTest(dependencies=dependencies):
                host = self.target("host_tests")
                host["dependencies"] = dependencies
                with self.assertRaisesRegex(ValueError, "dependency"):
                    build_mingw_host_only_tests.host_only_test_targets(
                        source_root, {"host_tests": host}, ["core/libs/tests/HostOnly.cpp"]
                    )

    def test_selects_only_changed_host_test_sources(self):
        first = self.target("first_tests")
        second = self.target("second_tests", source="core/libs/tests/Second.cpp")
        build_dir = self.fixture([first, second])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        selected = build_mingw_host_only_tests.host_only_test_targets(
            source_root, targets, ["core/libs/tests/Second.cpp"]
        )
        self.assertEqual(selected, ["second_tests"])

    def test_cmake_or_header_changes_check_all_host_targets(self):
        first = self.target("first_tests")
        second = self.target("second_tests", source="core/libs/tests/Second.cpp")
        build_dir = self.fixture([first, second])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(
            build_mingw_host_only_tests.host_only_test_targets(
                source_root, targets, ["core/libs/CMakeLists.txt"]
            ),
            ["first_tests", "second_tests"],
        )
        self.assertEqual(
            build_mingw_host_only_tests.host_only_test_targets(
                source_root, targets, ["core/libs/tests/HostOnly.hpp"]
            ),
            ["first_tests", "second_tests"],
        )

    def test_header_listed_as_a_target_source_still_selects_all_host_targets(self):
        first = self.target("first_tests")
        second = self.target("second_tests", source="core/libs/tests/Second.cpp")
        second["sources"].append({"path": "core/libs/tests/Shared.hpp"})
        build_dir = self.fixture([first, second])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(
            build_mingw_host_only_tests.host_only_test_targets(
                source_root, targets, ["core/libs/tests/Shared.hpp"]
            ),
            ["first_tests", "second_tests"],
        )

    def test_fails_when_cmake_did_not_write_a_file_api_index(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "no CMake File API index"):
                build_mingw_host_only_tests.read_reply(Path(directory))

    def test_wrong_shaped_file_api_documents_are_reported_cleanly(self):
        cases = (
            ("index", [], "CMake File API index"),
            ("index", {"reply": []}, "CMake File API reply"),
            ("index", {"reply": {"codemodel-v2": []}}, "CMake codemodel reference"),
            ("model", [], "CMake codemodel"),
            ("model", {"configurations": [[]], "paths": {"source": "."}}, "CMake configuration"),
            ("model", {"configurations": [{"targets": [[]]}], "paths": {"source": "."}}, "CMake target reference"),
            ("target", [], "CMake target"),
        )
        for location, document, expected in cases:
            with self.subTest(location=location, expected=expected), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                reply = root / "build" / ".cmake" / "api" / "v1" / "reply"
                reply.mkdir(parents=True)
                target = self.target("host_tests")
                target_name = "target-host_tests.json"
                (reply / target_name).write_text(json.dumps(target), encoding="utf-8")
                model = {"configurations": [{"targets": [{"id": target["id"], "jsonFile": target_name}]}],
                         "paths": {"source": str(root)}}
                (reply / "codemodel.json").write_text(json.dumps(model), encoding="utf-8")
                index = {"reply": {"codemodel-v2": {"jsonFile": "codemodel.json"}}}
                (reply / "index-0001.json").write_text(json.dumps(index), encoding="utf-8")
                path = {"index": reply / "index-0001.json", "model": reply / "codemodel.json",
                        "target": reply / target_name}[location]
                path.write_text(json.dumps(document), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, expected):
                    build_mingw_host_only_tests.read_reply(root / "build")
                errors = io.StringIO()
                with contextlib.redirect_stderr(errors):
                    self.assertEqual(build_mingw_host_only_tests.main([str(root / "build"), "--list-only"]), 1)
                self.assertIn(expected, errors.getvalue())
                self.assertNotIn("Traceback", errors.getvalue())

    def test_wrong_shaped_target_members_are_reported_cleanly(self):
        cases = (
            ({"name": []}, "CMake target name"),
            ({"link": []}, "CMake target link"),
            ({"link": {"commandFragments": ["bad"]}}, "CMake link command fragments"),
            ({"artifacts": ["bad"]}, "CMake target artifacts"),
            ({"sources": ["bad"]}, "CMake target sources"),
        )
        for changes, expected in cases:
            with self.subTest(expected=expected), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                reply = root / "build" / ".cmake" / "api" / "v1" / "reply"
                reply.mkdir(parents=True)
                target = self.target("host_tests")
                target.update(changes)
                (reply / "target.json").write_text(json.dumps(target), encoding="utf-8")
                model = {"configurations": [{"targets": [{"id": "host_tests", "jsonFile": "target.json"}]}],
                         "paths": {"source": str(root)}}
                (reply / "codemodel.json").write_text(json.dumps(model), encoding="utf-8")
                index = {"reply": {"codemodel-v2": {"jsonFile": "codemodel.json"}}}
                (reply / "index-0001.json").write_text(json.dumps(index), encoding="utf-8")
                errors = io.StringIO()
                with contextlib.redirect_stderr(errors):
                    self.assertEqual(build_mingw_host_only_tests.main([str(root / "build"), "--list-only"]), 1)
                self.assertIn(expected, errors.getvalue())
                self.assertNotIn("Traceback", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
