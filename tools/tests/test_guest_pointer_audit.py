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
    return [(entry["function"], entry["parameter"], entry["kinds"]) for entry in guest_pointer_audit.audit_text(guest_pointer_audit.source_code(source))]


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

    def test_array_parameters_are_pointer_parameters(self):
        source = (
            "int APS5_VABI sceExampleValues(const int values[]) { return values[0]; }\n"
            "int APS5_VABI sceExampleTable(const Info* const table[], int count) { return table[0]->size + count; }\n"
            "int APS5_VABI sceExampleSized(struct waitfd fds[4]) { fds[0].fd = 1; return 0; }\n"
            "int APS5_VABI sceExampleCopied(int values[]) { int first = 0;\n"
            "  if (!GuestCopyIn_nid_no_patch(&first, values, sizeof(first))) return 14; return first; }\n"
        )
        self.assertEqual(findings(source), [
            ("sceExampleValues", "values", ["index"]),
            ("sceExampleTable", "table", ["index"]),
            ("sceExampleSized", "fds", ["index"]),
        ])

    def test_accesses_through_a_cast_are_reported(self):
        source = (
            "void APS5_VABI sceExampleInit(void* self) { *static_cast<std::uint8_t*>(self) = 0; }\n"
            "int APS5_VABI sceExampleMember(void* info) { return reinterpret_cast<Info*>( info )->size; }\n"
            "int APS5_VABI sceExampleIndex(const void* values) { return static_cast<const int*>(values)[1]; }\n"
            "int APS5_VABI sceExampleConst(const Info* info) { const_cast<Info*>(info)->size = 0; return 0; }\n"
            "int APS5_VABI sceExampleNested(void* table) { return (*static_cast<std::map<int, std::vector<int>>*>(table))[0][0]; }\n"
            "int APS5_VABI sceExampleSize(void* out) { return static_cast<int>(sizeof(*static_cast<Info*>(out))); }\n"
            "int APS5_VABI sceExamplePassed(void* out) { return Fill(static_cast<Info*>(out)); }\n"
        )
        self.assertEqual(findings(source), [
            ("sceExampleInit", "self", ["dereference"]),
            ("sceExampleMember", "info", ["member"]),
            ("sceExampleIndex", "values", ["index"]),
            ("sceExampleConst", "info", ["member"]),
            ("sceExampleNested", "table", ["dereference"]),
        ])

    def test_accesses_through_a_cast_and_offset_are_reported(self):
        source = (
            "int APS5_VABI sceExampleOffset(const void* mode) { unsigned long long v;\n"
            "  memcpy(&v, static_cast<const unsigned char*>(mode) + 0x10, sizeof(v)); return static_cast<int>(v); }\n"
            "int APS5_VABI sceExampleParen(const void* mode) { unsigned int v;\n"
            "  memcpy(&v, (mode) + 4, sizeof(v)); return static_cast<int>(v); }\n"
        )
        self.assertEqual(findings(source), [
            ("sceExampleOffset", "mode", ["library call"]),
            ("sceExampleParen", "mode", ["library call"]),
        ])

    def test_an_offset_on_another_pointer_is_not_reported(self):
        source = (
            "int APS5_VABI sceExampleOther(const void* mode) { unsigned long long v;\n"
            "  memcpy(&v, other + 0x10, sizeof(v)); return static_cast<int>(v); }\n"
        )
        self.assertEqual(findings(source), [])

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

    def test_qualified_members_named_like_the_parameter_are_not_reported(self):
        source = (
            "int APS5_VABI sceExampleMember(std::uint32_t* devices, Service& service) { return service.devices[0]; }\n"
            "int APS5_VABI sceExampleNested(std::uint32_t* slots, const Log& log) { return log.slots[1]; }\n"
            "int APS5_VABI sceExampleBoth(std::uint32_t* devices, Service& service) { devices[1] = service.devices[0]; return 0; }\n"
        )
        self.assertEqual(findings(source), [("sceExampleBoth", "devices", ["index"])])

    def test_evaluated_guest_copy_pointer_argument_is_reported(self):
        source = (
            "int APS5_VABI sceExampleNested(int** guest) { int out = 0; "
            "GuestCopyIn_nid_no_patch(&out, *guest, sizeof(out)); return out; }\n"
            "int APS5_VABI sceExampleWrapped(int** guest) { int out = 0; "
            "GuestCopyIn_nid_no_patch(&out, Wrap(*guest), sizeof(out)); return out; }\n"
        )
        self.assertEqual(findings(source), [
            ("sceExampleNested", "guest", ["dereference"]),
            ("sceExampleWrapped", "guest", ["dereference"]),
        ])

    def test_unevaluated_guest_copy_pointer_argument_is_not_reported(self):
        source = (
            "int APS5_VABI sceExampleSize(int** guest) { int out = 0; "
            "GuestCopyIn_nid_no_patch(&out, sizeof(*guest) ? guest : guest, sizeof(out)); return out; }\n"
            "int APS5_VABI sceExampleCallSize(int** guest) { int out = 0; "
            "return sizeof(GuestCopyIn_nid_no_patch(&out, *guest, sizeof(out))); }\n"
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
            "int APS5_VABI sceExampleArray(Info* out) { const char v[4]{};\n"
            "  return GuestCopyOut_nid_no_patch(out->name, v, sizeof(v)) ? 0 : 14; }\n"
            "int APS5_VABI sceExampleSize(Info* info) { return static_cast<int>(sizeof(*info) + alignof(decltype(info[0]))); }\n"
        )
        self.assertEqual(findings(source), [])

    def test_evaluated_safe_call_arguments_are_accesses(self):
        source = (
            "int APS5_VABI sceExampleSize(Info* p) { Info local{}; return GuestCopyIn_nid_no_patch(&local, p, p->size); }\n"
            "int APS5_VABI sceExampleLength(const char* p) { char v[8]{}; return GuestCopyIn_nid_no_patch(v, p, std::strlen(p)); }\n"
        )
        self.assertEqual(findings(source), [("sceExampleSize", "p", ["member"]), ("sceExampleLength", "p", ["library call"])])

    def test_comparisons_in_call_arguments_do_not_hide_operands(self):
        source = "int APS5_VABI sceExampleClamp(int* p, int n, int limit) { int source[2]{}; std::copy_n(source, n < limit ? n : limit, p); return 0; }\n"
        self.assertEqual(findings(source), [("sceExampleClamp", "p", ["library call"])])

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

    def test_format_strings_are_pointer_operands(self):
        source = (
            "int APS5_VABI sceExampleSn(const char* p) { char values[32]{}; return std::snprintf(values, sizeof(values), p, 1); }\n"
            "int APS5_VABI sceExampleS(const char* p) { char values[32]{}; return sprintf(values, p, 1); }\n"
            "int APS5_VABI sceExampleValue(const char* p) { char values[32]{}; return std::snprintf(values, sizeof(values), \"%p\", p); }\n"
        )
        self.assertEqual(findings(source), [("sceExampleSn", "p", ["library call"]), ("sceExampleS", "p", ["library call"])])

    def test_line_is_the_earliest_access_of_any_kind(self):
        entry, = guest_pointer_audit.audit_text(guest_pointer_audit.source_code("int APS5_VABI sceExampleEarly(Info* p)\n{\n  p[0] = {};\n  p->field = 1;\n  return 0;\n}\n"))
        self.assertEqual((entry["line"], entry["kinds"]), (3, ["index", "member"]))

    def test_comments_and_literals_are_ignored(self):
        source = "int APS5_VABI sceExampleText(Info* info) { // info->size = 4;\n  return Log(\"info->size *info info[0]\"); }\n"
        self.assertEqual(findings(source), [])

    def test_line_counts_blank_spliced_and_commented_lines(self):
        source = ("int APS5_VABI sceExampleBlank(int* out)\n{\n\n\n  /* one\n     two */\n  const char* s = R\"(a\nb)\";\n"
                  "  int wide = 1 + \\\n    2;\n\n  *out = wide;\n  return 0;\n}\n")
        entry, = guest_pointer_audit.audit_text(guest_pointer_audit.source_code(source))
        self.assertEqual(entry["line"], 12)

    def test_line_points_at_the_first_access(self):
        entry, = guest_pointer_audit.audit_text(guest_pointer_audit.source_code("int APS5_VABI sceExampleLine(int* out)\n{\n  int a = 0;\n  *out = a;\n  return 0;\n}\n"))
        self.assertEqual(entry["line"], 4)


class LinkageTests(unittest.TestCase):
    def test_anonymous_namespace_callback_is_not_reported(self):
        source = (
            "namespace {\n"
            "int APS5_VABI LibraryGetFaceScale(Metrics* out) { out->scale = 1; return 0; }\n"
            "}\n"
        )
        self.assertEqual(findings(source), [])

    def test_static_callback_is_not_reported(self):
        source = "static int APS5_VABI defaultAllocate(void* out, std::size_t size) { std::memset(out, 0, size); return 0; }\n"
        self.assertEqual(findings(source), [])

    def test_named_namespace_definition_is_reported(self):
        source = (
            "namespace detail {\n"
            "int APS5_VABI sceExampleGet(Info* info) { info->size = 1; return 0; }\n"
            "}\n"
        )
        self.assertEqual(findings(source), [("sceExampleGet", "info", ["member"])])

    def test_static_variable_in_a_previous_definition_does_not_hide_the_next_one(self):
        source = (
            "int APS5_VABI sceExampleFirst(int* out) { static int counter = 0; *out = counter; return 0; }\n"
            "int APS5_VABI sceExampleSecond(Info* info) { info->size = 2; return 0; }\n"
        )
        self.assertEqual(findings(source), [
            ("sceExampleFirst", "out", ["dereference"]),
            ("sceExampleSecond", "info", ["member"]),
        ])

    def test_exported_definition_outside_a_namespace_is_reported(self):
        source = "int APS5_VABI sceExamplePlain(std::uint32_t* out) { *out = 3; return 0; }\n"
        self.assertEqual(findings(source), [("sceExamplePlain", "out", ["dereference"])])


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

    def test_library_selection_cannot_escape_prx(self):
        files = {
            "libSceExample/Export.cpp": "int APS5_VABI sceExample(int* out) { *out = 1; return 0; }\n",
            "../stray/Export.cpp": "int APS5_VABI sceOutside(int* out) { *out = 2; return 0; }\n",
        }
        for name in ("..", "../stray", "libSceExample/.."):
            with self.subTest(name=name):
                with contextlib.redirect_stderr(io.StringIO()) as errors, self.assertRaises(SystemExit) as raised:
                    self.run_main(["--json", name], files)
                self.assertEqual(raised.exception.code, 2)
                self.assertIn("not a library under core/libs/prx:", errors.getvalue())


    def test_split_top_level_ignores_commas_inside_brackets(self):
        parts = guest_pointer_audit.split_top_level("a, f(b, c), [d, e]")
        self.assertEqual([text for _, text in parts], ["a", " f(b, c)", " [d, e]"])
        self.assertEqual(parts[0][0], 0)


    def test_split_top_level_angles_flag_controls_angle_nesting(self):
        text = "a<b, c>, d"
        with_angles = [part for _, part in guest_pointer_audit.split_top_level(text)]
        self.assertEqual(with_angles, ["a<b, c>", " d"])
        without = [part for _, part in guest_pointer_audit.split_top_level(text, angles=False)]
        self.assertEqual(without, ["a<b", " c>", " d"])
class ClosingTests(unittest.TestCase):
    def test_closing_matches_nested_parentheses(self):
        text = "f(a(b), c) x"
        self.assertEqual(guest_pointer_audit.closing(text, 1), 9)
        self.assertEqual(text[9], ")")
        self.assertEqual(guest_pointer_audit.closing("f(a", 1), len("f(a") - 1)


class NamesParameterTests(unittest.TestCase):
    def test_names_parameter_matches_whole_names_only(self):
        self.assertTrue(guest_pointer_audit.names_parameter("buf", "buf"))
        self.assertTrue(guest_pointer_audit.names_parameter("  buf ", "buf"))
        self.assertFalse(guest_pointer_audit.names_parameter("bufx", "buf"))

class PointerParameterTests(unittest.TestCase):
    def test_pointer_parameters_lists_pointer_names_only(self):
        self.assertEqual(guest_pointer_audit.pointer_parameters("int count, const char* text, uint32_t* out, int& ref"),
                         ["text", "out"])

    def test_pointer_parameters_ignores_defaults_and_function_pointers(self):
        self.assertEqual(guest_pointer_audit.pointer_parameters("int* fallback = nullptr, void (*callback)(int), int value"),
                         ["fallback"])



class SpliceTests(unittest.TestCase):
    def test_splice_lines_joins_continued_lines_and_keeps_line_count(self):
        self.assertEqual(guest_pointer_audit.splice_lines("a \\\nb\nc"), "a b\n\nc")

    def test_splice_lines_normalises_crlf(self):
        self.assertEqual(guest_pointer_audit.splice_lines("one\r\ntwo"), "one\ntwo")



class SourceCodeTests(unittest.TestCase):
    def test_source_code_blanks_line_comments_and_keeps_code(self):
        text = "a = 1; // hi\n"
        blanked = guest_pointer_audit.source_code(text)
        self.assertEqual(blanked.rstrip("\n"), "a = 1;" + " " * 6)
        self.assertEqual(len(blanked), len(text))

    def test_source_code_replaces_string_literals(self):
        self.assertEqual(guest_pointer_audit.source_code('x = "ab";\n'), "x = LITERAL;\n")



class AccessKindTests(unittest.TestCase):
    def test_accesses_reports_each_kind_for_the_named_pointer(self):
        kinds = [kind for _, kind in guest_pointer_audit.accesses("p->x; *p = 1; p[2] = 0; q->y;", "p")]
        self.assertEqual(kinds, ["member", "dereference", "index"])

    def test_accesses_ignores_other_names(self):
        self.assertEqual(guest_pointer_audit.accesses("q->x; q[0] = 1;", "p"), [])

class QuietTests(unittest.TestCase):
    def test_quiet_blanks_the_operand_of_sizeof_keeping_the_width(self):
        text = "sizeof(p->x)"
        blanked = guest_pointer_audit.quiet(text)
        self.assertEqual(blanked, "sizeof(" + " " * 4 + ")")
        self.assertEqual(len(blanked), len(text))

    def test_quiet_blanks_only_the_listed_arguments_of_safe_copies(self):
        blanked = guest_pointer_audit.quiet("GuestCopyIn_nid_no_patch(dst, p->x, n)")
        self.assertNotIn("p->x", blanked)
        self.assertIn("n", blanked)
        self.assertEqual(guest_pointer_audit.quiet("GuestCopyIn_nid_no_patch(a, b)", safe=False), "GuestCopyIn_nid_no_patch(a, b)")


class CheckedDereferenceTests(unittest.TestCase):
    def test_checked_argument_dereferences_reports_a_dereference_in_a_safe_copy(self):
        found = guest_pointer_audit.checked_argument_dereferences("GuestCopyIn_nid_no_patch(*p, q, 1)", "p")
        self.assertEqual([kind for _, kind in found], ["dereference"])

    def test_checked_argument_dereferences_ignores_a_plain_pointer_argument(self):
        self.assertEqual(guest_pointer_audit.checked_argument_dereferences("GuestCopyIn_nid_no_patch(p, q, 1)", "p"), [])

    def test_internal_linkage_detects_static_and_anonymous_namespace_definitions(self):
        text = "static int a() {}\nint b() {}\nnamespace { int c() {} }\n"
        spans = guest_pointer_audit.anonymous_spans(text)
        self.assertEqual(len(spans), 1)
        self.assertTrue(guest_pointer_audit.internal_linkage(text, text.index("int a"), spans))
        self.assertFalse(guest_pointer_audit.internal_linkage(text, text.index("int b"), spans))
        self.assertTrue(guest_pointer_audit.internal_linkage(text, text.index("int c"), spans))


class SourceCodeBlankingTests(unittest.TestCase):
    def test_comments_become_spaces_and_literals_become_a_placeholder(self):
        text = 'int a; /* c1\nc2 */ const char *s = "x\\ny"; // tail\nint b;\n'
        code = guest_pointer_audit.source_code(text)
        self.assertEqual(code.count("\n"), text.count("\n"))
        self.assertIn("LITERAL", code)
        for gone in ("c1", "c2", "tail", "x\\ny"):
            self.assertNotIn(gone, code)
        for kept in ("int a;", "const char *s =", "int b;"):
            self.assertIn(kept, code)

    def test_multi_line_literal_keeps_its_line_count(self):
        text = 'f("a\\\nb");\ng();\n'
        code = guest_pointer_audit.source_code(text)
        self.assertEqual(code.count("\n"), text.count("\n"))
        self.assertIn("g();", code)

    def test_digit_separators_and_character_literals_do_not_hide_following_code(self):
        source = (
            "int APS5_VABI sceExampleQuotes(int* out) {\n"
            "  int n = 1'000 + 0x1F'FF;\n"
            "  int c = x'\\n';\n"
            '  const char* s = 9.L"/* not a comment";\n'
            "  *out = n + c;\n"
            "  return 0;\n"
            "}\n"
        )
        entry, = guest_pointer_audit.audit_text(guest_pointer_audit.source_code(source))
        self.assertEqual((entry["function"], entry["parameter"], entry["line"], entry["kinds"]),
                         ("sceExampleQuotes", "out", 5, ["dereference"]))

    def test_multiple_parameters_report_each_parameters_earliest_line(self):
        source = (
            "int APS5_VABI sceExampleMulti(Info* first, int* second) {\n"
            "  *second = 1;\n"
            "  first->size = 2;\n"
            "  return 0;\n"
            "}\n"
        )
        entries = guest_pointer_audit.audit_text(guest_pointer_audit.source_code(source))
        self.assertEqual([(e["parameter"], e["line"], e["kinds"]) for e in entries], [
            ("first", 3, ["member"]),
            ("second", 2, ["dereference"]),
        ])

    def test_line_spliced_vabi_definition_is_still_audited(self):
        source = "int APS5_\\\nVABI sceExampleSplit(int* out) {\n  *out = 1;\n  return 0;\n}\n"
        entry, = guest_pointer_audit.audit_text(guest_pointer_audit.source_code(source))
        self.assertEqual((entry["function"], entry["parameter"], entry["line"], entry["kinds"]),
                         ("sceExampleSplit", "out", 3, ["dereference"]))


if __name__ == "__main__":
    unittest.main()
