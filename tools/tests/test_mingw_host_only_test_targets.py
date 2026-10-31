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

    def test_selects_host_test_linking_root_declared_native_helper(self):
        helper = self.target("host_thread_local_test_library", target_type="SHARED_LIBRARY",
                             source="core/libs/tests/HostThreadLocalLibrary.cpp")
        helper["artifacts"] = [{"path": "core/libs/libhost_thread_local_test_library.dll.a"}]
        helper["id"] = "helper-id"
        host = self.target("host_thread_local_tests", dependencies=("helper-id",),
                           source="core/libs/tests/HostThreadLocal.cpp",
                           fragment="/build/core/libs/libhost_thread_local_test_library.dll.a")
        build_dir = self.fixture([helper, host])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(
            build_mingw_host_only_tests.host_only_test_targets(source_root, targets),
            ["host_thread_local_tests"],
        )

    def test_selects_host_test_linking_nested_test_directory_helper(self):
        helper = self.target("nested_helper", target_type="STATIC_LIBRARY",
                             source="core/libs/prx/custom/tests/NestedHelper.cpp")
        helper["paths"]["source"] = "core/libs/prx/custom"
        helper["artifacts"] = [{"path": "core/libs/prx/custom/libnested_helper.a"}]
        helper["id"] = "nested-helper-id"
        host = self.target("nested_helper_tests", dependencies=("nested-helper-id",),
                           source="core/libs/tests/NestedTests.cpp",
                           fragment="/build/core/libs/prx/custom/libnested_helper.a")
        build_dir = self.fixture([helper, host])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(
            build_mingw_host_only_tests.host_only_test_targets(source_root, targets),
            ["nested_helper_tests"],
        )

    def test_excludes_library_with_mixed_test_and_product_sources(self):
        mixed = self.target("mixed_lib", target_type="STATIC_LIBRARY",
                            source="core/libs/tests/Helper.cpp")
        mixed["sources"].append({"path": "core/libs/src/Product.cpp"})
        mixed["artifacts"] = [{"path": "core/libs/libmixed_lib.a"}]
        mixed["id"] = "mixed-id"
        host = self.target("mixed_dependent_tests", dependencies=("mixed-id",),
                           source="core/libs/tests/HostOnly.cpp",
                           fragment="/build/core/libs/libmixed_lib.a")
        build_dir = self.fixture([mixed, host])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(
            build_mingw_host_only_tests.host_only_test_targets(source_root, targets),
            [],
        )

    def test_excludes_host_test_when_helper_transitively_links_product_library(self):
        product = self.target("product_lib", target_type="STATIC_LIBRARY",
                              source="core/libs/utility/src/Utility.cpp")
        product["paths"]["source"] = "core/libs/utility"
        product["artifacts"] = [{"path": "core/libs/utility/libproduct_lib.a"}]
        product["id"] = "product-id"
        helper = self.target("test_helper", target_type="SHARED_LIBRARY",
                             source="core/libs/tests/Helper.cpp",
                             fragment="/build/core/libs/utility/libproduct_lib.a")
        helper["artifacts"] = [{"path": "core/libs/tests/libtest_helper.dll.a"}]
        helper["id"] = "helper-id"
        host = self.target("host_tests", dependencies=("helper-id",),
                           source="core/libs/tests/HostOnly.cpp",
                           fragment="/build/core/libs/tests/libtest_helper.dll.a")
        build_dir = self.fixture([product, helper, host])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(
            build_mingw_host_only_tests.host_only_test_targets(source_root, targets),
            [],
        )

    def test_excludes_host_test_when_helper_links_guest_libc_without_libc_target(self):
        helper = self.target("test_helper", target_type="SHARED_LIBRARY",
                             source="core/libs/tests/Helper.cpp",
                             fragment="C:/build/core/libs/prx/libc/libc.dll.a")
        helper["artifacts"] = [{"path": "core/libs/tests/libtest_helper.dll.a"}]
        helper["id"] = "helper-id"
        host = self.target("host_tests", dependencies=("helper-id",),
                           source="core/libs/tests/HostOnly.cpp",
                           fragment="/build/core/libs/tests/libtest_helper.dll.a")
        build_dir = self.fixture([helper, host])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(
            build_mingw_host_only_tests.host_only_test_targets(source_root, targets),
            [],
        )

    def test_ignores_helper_order_only_dependency_on_product_target(self):
        product = self.target("product_lib", target_type="STATIC_LIBRARY",
                              source="core/libs/utility/src/Utility.cpp")
        product["paths"]["source"] = "core/libs/utility"
        product["artifacts"] = [{"path": "core/libs/utility/libproduct_lib.a"}]
        product["id"] = "product-id"
        helper = self.target("test_helper", target_type="SHARED_LIBRARY",
                             source="core/libs/tests/Helper.cpp",
                             dependencies=("product-id",))
        helper["artifacts"] = [{"path": "core/libs/tests/libtest_helper.dll.a"}]
        helper["id"] = "helper-id"
        host = self.target("host_tests", dependencies=("helper-id",),
                           source="core/libs/tests/HostOnly.cpp",
                           fragment="/build/core/libs/tests/libtest_helper.dll.a")
        build_dir = self.fixture([product, helper, host])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(
            build_mingw_host_only_tests.host_only_test_targets(source_root, targets),
            ["host_tests"],
        )

    def test_safely_handles_finite_helper_link_cycle(self):
        helper_a = self.target("helper_a", target_type="SHARED_LIBRARY",
                               source="core/libs/tests/HelperA.cpp",
                               fragment="/build/core/libs/tests/libhelper_b.dll.a")
        helper_a["artifacts"] = [{"path": "core/libs/tests/libhelper_a.dll.a"}]
        helper_a["id"] = "helper-a-id"
        helper_b = self.target("helper_b", target_type="SHARED_LIBRARY",
                               source="core/libs/tests/HelperB.cpp",
                               fragment="/build/core/libs/tests/libhelper_a.dll.a")
        helper_b["artifacts"] = [{"path": "core/libs/tests/libhelper_b.dll.a"}]
        helper_b["id"] = "helper-b-id"
        host = self.target("cycle_tests", dependencies=("helper-a-id",),
                           source="core/libs/tests/HostOnly.cpp",
                           fragment="/build/core/libs/tests/libhelper_a.dll.a")
        build_dir = self.fixture([helper_a, helper_b, host])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(
            build_mingw_host_only_tests.host_only_test_targets(source_root, targets),
            ["cycle_tests"],
        )

    def test_falls_back_to_declaration_directory_when_library_has_no_sources(self):
        test_dir_lib = self.target("test_dir_lib", target_type="STATIC_LIBRARY")
        test_dir_lib["paths"]["source"] = "core/libs/tests"
        test_dir_lib["sources"] = []
        test_dir_lib["artifacts"] = [{"path": "core/libs/tests/libtest_dir_lib.a"}]
        test_dir_lib["id"] = "test-dir-lib-id"
        parent_dir_lib = self.target("parent_dir_lib", target_type="STATIC_LIBRARY")
        parent_dir_lib["paths"]["source"] = "core/libs"
        parent_dir_lib["sources"] = []
        parent_dir_lib["artifacts"] = [{"path": "core/libs/libparent_dir_lib.a"}]
        parent_dir_lib["id"] = "parent-dir-lib-id"
        host_ok = self.target("host_ok", dependencies=("test-dir-lib-id",),
                              source="core/libs/tests/HostOnly.cpp",
                              fragment="/build/core/libs/tests/libtest_dir_lib.a")
        host_excluded = self.target("host_excluded", dependencies=("parent-dir-lib-id",),
                                    source="core/libs/tests/HostOnly.cpp",
                                    fragment="/build/core/libs/libparent_dir_lib.a")
        build_dir = self.fixture([test_dir_lib, parent_dir_lib, host_ok, host_excluded])
        source_root, targets = build_mingw_host_only_tests.read_reply(build_dir)
        self.assertEqual(
            build_mingw_host_only_tests.host_only_test_targets(source_root, targets),
            ["host_ok"],
        )

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


if __name__ == "__main__":
    unittest.main()
