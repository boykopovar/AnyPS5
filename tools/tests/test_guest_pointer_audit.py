import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import guest_pointer_audit
import progress


def findings(source):
    return [(entry["function"], entry["parameter"], entry["kinds"]) for entry in guest_pointer_audit.audit_text(progress.code(source))]


class AccessTests(unittest.TestCase):
    def test_each_direct_access_kind_is_reported(self):
        source = (
            "int APS5_VABI sceExampleMember(Info* info) { info->size = 4; return 0; }\n"
            "int APS5_VABI sceExampleIndex(std::uint32_t* values) { values[1] = 0; return 0; }\n"
            "int APS5_VABI sceExampleDereference(std::uint64_t* out) { *out = 7; return 0; }\n"
            "int APS5_VABI sceExampleCopy(void* out) { const int v = 1; std::memcpy(out, &v, sizeof(v)); return 0; }\n"
            "int APS5_VABI sceExampleLength(const char* path) { return static_cast<int>(std::strlen(path)); }\n"
            "int APS5_VABI sceExampleRead(const Attr* const* attr) { if (!*attr) return 1; return 0; }\n"
        )
        self.assertEqual(findings(source), [
            ("sceExampleMember", "info", ["member"]),
            ("sceExampleIndex", "values", ["index"]),
            ("sceExampleDereference", "out", ["dereference"]),
            ("sceExampleCopy", "out", ["library call"]),
            ("sceExampleLength", "path", ["library call"]),
            ("sceExampleRead", "attr", ["dereference"]),
        ])

    def test_safe_helpers_and_non_pointer_parameters_are_not_reported(self):
        source = (
            "int APS5_VABI sceExampleOut(std::uint64_t* out) { const std::uint64_t v = 7;\n"
            "  if (!GuestCopyOut_nid_no_patch(out, &v, sizeof(v))) return 14; return 0; }\n"
            "int APS5_VABI sceExampleIn(const Info* info) { Info copy{};\n"
            "  if (!GuestCopyIn_nid_no_patch(&copy, info, sizeof(copy))) return 14; return copy.size; }\n"
            "int APS5_VABI sceExamplePath(const char* path) { std::string s; return CopyGuestPath_nid_no_patch(path, s); }\n"
            "int APS5_VABI sceExampleCallback(void (*callback)(int), int value) { callback(value); return value * 2; }\n"
            "int APS5_VABI sceExampleReference(Info& info) { info.size = 1; return 0; }\n"
            "int APS5_VABI sceExampleNull(void* out) { return out == nullptr ? 22 : 0; }\n"
        )
        self.assertEqual(findings(source), [])

    def test_a_safe_copy_does_not_hide_other_direct_accesses(self):
        source = (
            "int APS5_VABI sceExampleMixed(Info* p) { Info local{};\n"
            "  GuestCopyIn_nid_no_patch(&local, p, sizeof(local)); p[1] = local; return 0; }\n"
            "int APS5_VABI sceExampleSizeof(int* p, int* q) { int v = 0;\n"
            "  GuestCopyOut_nid_no_patch(q, &v, sizeof(*p)); *p = 1; return 0; }\n"
        )
        self.assertEqual(findings(source), [("sceExampleMixed", "p", ["index"]), ("sceExampleSizeof", "p", ["dereference"])])

    def test_safe_call_arguments_and_unevaluated_operands_are_not_accesses(self):
        source = (
            "int APS5_VABI sceExampleField(Info* out) { const int v = 1;\n"
            "  return GuestCopyOut_nid_no_patch(&out->size, &v, sizeof(out->size)) ? 0 : 14; }\n"
            "int APS5_VABI sceExampleSize(Info* info) { return static_cast<int>(sizeof(*info) + alignof(decltype(info[0]))); }\n"
        )
        self.assertEqual(findings(source), [])

    def test_every_pointer_operand_of_a_copy_or_fill_is_checked(self):
        source = (
            "int APS5_VABI sceExampleCopyTo(const int* first, int* p) { std::copy(first, first + 2, p); return 0; }\n"
            "int APS5_VABI sceExampleCopyN(int* p) { int v[2]{}; std::copy_n(v, 2, reinterpret_cast<int*>(p)); return 0; }\n"
            "int APS5_VABI sceExampleNested(char* p, int n) { std::memset(Offset(n), 0, Size(p, n)); return 0; }\n"
            "int APS5_VABI sceExampleFillValue(int** p) { int* values[2]{}; std::fill(values, values + 2, p[0]); return 0; }\n"
        )
        self.assertEqual(findings(source), [
            ("sceExampleCopyTo", "first", ["library call"]),
            ("sceExampleCopyTo", "p", ["library call"]),
            ("sceExampleCopyN", "p", ["library call"]),
            ("sceExampleFillValue", "p", ["index"]),
        ])

    def test_line_is_the_earliest_access_of_any_kind(self):
        entry, = guest_pointer_audit.audit_text(progress.code("int APS5_VABI sceExampleEarly(Info* p)\n{\n  p[0] = {};\n  p->field = 1;\n  return 0;\n}\n"))
        self.assertEqual((entry["line"], entry["kinds"]), (3, ["index", "member"]))

    def test_comments_and_literals_are_ignored(self):
        source = "int APS5_VABI sceExampleText(Info* info) { // info->size = 4;\n  return Log(\"info->size *info info[0]\"); }\n"
        self.assertEqual(findings(source), [])

    def test_line_points_at_the_first_access(self):
        entry, = guest_pointer_audit.audit_text(progress.code("int APS5_VABI sceExampleLine(int* out)\n{\n  int a = 0;\n  *out = a;\n  return 0;\n}\n"))
        self.assertEqual(entry["line"], 4)


class CommandLineTests(unittest.TestCase):
    def run_main(self, argv, files):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, text in files.items():
                path = root / "core" / "libs" / "prx" / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="utf-8")
            output = io.StringIO()
            with mock.patch.object(guest_pointer_audit, "ROOT", root), mock.patch.object(guest_pointer_audit, "PRX", root / "core" / "libs" / "prx"), contextlib.redirect_stdout(output):
                code = guest_pointer_audit.main(argv)
            return code, output.getvalue()

    def test_json_lists_findings_and_skips_tests(self):
        files = {
            "libSceExample/Export.cpp": "int APS5_VABI sceExampleGet(int* out) { *out = 1; return 0; }\n",
            "libSceExample/tests/Example.cpp": "int APS5_VABI sceExampleTest(int* out) { *out = 1; return 0; }\n",
        }
        code, output = self.run_main(["--json", "libSceExample"], files)
        self.assertEqual(code, 0)
        self.assertEqual(json.loads(output), [{"library": "libSceExample", "file": "core/libs/prx/libSceExample/Export.cpp",
                                              "function": "sceExampleGet", "parameter": "out", "line": 1, "kinds": ["dereference"]}])

    def test_text_report_counts_per_library(self):
        code, output = self.run_main([], {"libSceExample/Export.cpp": "int APS5_VABI sceExampleGet(int* out) { out[0] = 1; return 0; }\n"})
        self.assertEqual(code, 0)
        self.assertIn("core/libs/prx/libSceExample/Export.cpp:1: sceExampleGet: parameter out (index)", output)
        self.assertIn("1 direct guest pointer accesses in 1 exported functions", output)

    def test_unknown_library_is_a_usage_error(self):
        with contextlib.redirect_stderr(io.StringIO()) as errors, self.assertRaises(SystemExit) as raised:
            self.run_main(["libSceMissing"], {"libSceExample/Export.cpp": ""})
        self.assertEqual(raised.exception.code, 2)
        self.assertIn("not a library under core/libs/prx: libSceMissing", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
