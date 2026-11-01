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

    def test_excludes_mingw_libc_import_library_from_host_only_targets(self):
        guest = self.target("guest_mktime_tests", source="core/libs/tests/GuestMktime.cpp",
                            fragment="C:/build/core/libs/libs/implib/liblibc.dll.a")
        source_root = Path(self.temporary.name).resolve()
        self.assertEqual(build_mingw_host_only_tests.host_only_test_targets(source_root, {guest["id"]: guest}), [])

    def test_libc_link_does_not_accept_similarly_named_libraries(self):
        for fragment in ("/build/notliblibc.dll.a", "/build/liblibc.dll.a.extra", "/build/liblibc.prx"):
            with self.subTest(fragment=fragment):
                target = self.target("host_tests", fragment=fragment)
                self.assertFalse(build_mingw_host_only_tests.links_guest_libc(target))

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


    def test_library_filename_substring_does_not_exclude_a_host_test(self):
        product = self.target("product_lib", target_type="STATIC_LIBRARY", source="core/libs/utility/src/Utility.cpp")
        product["artifacts"] = [{"path": "core/libs/utility/libproduct_lib.a"}]
        host = self.target("host_tests", fragment="C:/build/external/libprefix_libproduct_lib.a")
        guest = self.target("product_tests", fragment='"C:/build/core/libs/utility/libproduct_lib.a"')
        source_root, targets = build_mingw_host_only_tests.read_reply(self.fixture([product, host, guest]))
        self.assertEqual(build_mingw_host_only_tests.host_only_test_targets(source_root, targets), ["host_tests"])

    def assert_exact_file_argument_links_product(self, fragment):
        product = self.target("product_lib", target_type="STATIC_LIBRARY", source="core/libs/utility/src/Utility.cpp")
        product["artifacts"] = [{"path": "core/libs/utility/libproduct_lib.a"}]
        guest = self.target("product_tests", fragment=fragment)
        source_root, targets = build_mingw_host_only_tests.read_reply(self.fixture([product, guest]))
        self.assertEqual(build_mingw_host_only_tests.host_only_test_targets(source_root, targets), [])

    def test_gnu_exact_file_linker_argument_links_a_product_library(self):
        self.assert_exact_file_argument_links_product("-l:libproduct_lib.a")

    def test_gnu_exact_file_linker_argument_through_wl_links_a_product_library(self):
        self.assert_exact_file_argument_links_product("-Wl,-l:libproduct_lib.a")


if __name__ == "__main__":
    unittest.main()
