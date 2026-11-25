import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import agc_test_split

TOOL = Path(__file__).resolve().parents[1] / "agc_test_split.py"
LISTS = agc_test_split.LISTS
EXECUTION = ["agc_driver_image_tests"]
FRAGMENTS = agc_test_split.FRAGMENTS

ALPHA = """    add_test_executable(agc_alpha_tests tests/Alpha.cpp)
    target_link_libraries(agc_alpha_tests PRIVATE libc)
    add_test(NAME agc_alpha COMMAND agc_alpha_tests)
    set_tests_properties(agc_alpha PROPERTIES TIMEOUT 20)
"""
BETA = """    add_test_executable(agc_beta_tests tests/Beta.cpp)
    foreach(mode IN ITEMS one two)
        add_test(NAME agc_beta_${mode} COMMAND agc_beta_tests ${mode})
    endforeach()
    if(WIN32)
        set_tests_properties(agc_beta_one agc_beta_two PROPERTIES TIMEOUT 40)
    endif()
"""
DELTA = """    add_test_executable(agc_delta_tests tests/Delta.cpp)
    add_test(NAME agc_delta COMMAND agc_delta_tests)
"""


def original(blocks=(ALPHA, BETA), items="agc_gamma_tests", tweaks="", image=True, library="Export.cpp"):
    image_case = ("        if(agcTestName STREQUAL \"agc_driver_image\")\n"
                  "            set_tests_properties(${agcTestName} PROPERTIES TIMEOUT 30)\n"
                  "        endif()\n") if image else ""
    image_tweak = "    set_tests_properties(agc_driver_image PROPERTIES SKIP_RETURN_CODE 77)\n" if image else ""
    return (f"add_library(libSceAgcDriver SHARED EXCLUDE_FROM_ALL {library})\n\n"
            "if(BUILD_TESTING)\n" + "\n".join(blocks) + "endif()\n\n"
            "add_test_executable(agc_gamma_tests EXCLUDE_FROM_ALL tests/Gamma.cpp)\n"
            "target_link_libraries(agc_gamma_tests PRIVATE libc)\n\n"
            "file(GLOB agcExecutionTestSources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/execution/*.cpp)\n"
            "set(agcExecutionTests)\n"
            "foreach(agcExecutionTestSource IN LISTS agcExecutionTestSources)\n"
            "    get_filename_component(agcExecutionTestName ${agcExecutionTestSource} NAME_WE)\n"
            "    string(TOLOWER \"agc_driver_${agcExecutionTestName}_tests\" agcExecutionTest)\n"
            "    add_test_executable(${agcExecutionTest} EXCLUDE_FROM_ALL ${agcExecutionTestSource})\n"
            "    list(APPEND agcExecutionTests ${agcExecutionTest})\n"
            "endforeach()\n"
            "if(BUILD_TESTING)\n"
            f"    foreach(agcTest IN ITEMS {items} ${{agcExecutionTests}})\n"
            "        if(agcTest STREQUAL \"agc_driver_skipped_tests\")\n"
            "            continue()\n"
            "        endif()\n"
            "        string(REGEX REPLACE \"_tests$\" \"\" agcTestName \"${agcTest}\")\n"
            "        set_target_properties(${agcTest} PROPERTIES EXCLUDE_FROM_ALL OFF)\n"
            "        add_test(NAME ${agcTestName} COMMAND ${agcTest})\n" + image_case +
            "    endforeach()\n" + image_tweak + tweaks +
            "endif()\n\n"
            "if(BUILD_TESTING)\n"
            "    get_directory_property(agcRegisteredTests TESTS)\n"
            "    set_property(TEST ${agcRegisteredTests} APPEND PROPERTY ENVIRONMENT \"X=1\")\n"
            "endif()\n")


class SplitTextTests(unittest.TestCase):
    def test_split_moves_properties_for_a_multiline_bracket_test_name(self):
        property_call = "set_tests_properties([=[\nagc_alpha]=] PROPERTIES TIMEOUT 20)"
        alpha = ALPHA.replace("set_tests_properties(agc_alpha PROPERTIES TIMEOUT 20)", property_call)
        lists, fragments = agc_test_split.migrated(original(blocks=(alpha, BETA)), EXECUTION)
        self.assertIn(property_call, fragments["agc_alpha_tests"])
        self.assertNotIn(property_call, lists)

    def test_refactor_registers_default_tests_through_a_function(self):
        text = agc_test_split.refactor(original())
        self.assertIn("function(agc_add_default_test agcTest)", text)
        self.assertIn("    agc_add_default_test(agc_gamma_tests)\n", text)
        self.assertIn("    set_tests_properties(agc_driver_image PROPERTIES TIMEOUT 30)\n", text)
        self.assertIn("        agc_add_default_test(${agcTest})\n", text)
        self.assertNotIn("${agcExecutionTests})", text)
        self.assertLess(text.index("function(agc_add_default_test"), text.index("agc_add_default_test(agc_gamma_tests)"))
        self.assertEqual(agc_test_split.refactor(text), text)

    def test_split_writes_one_fragment_per_test_executable(self):
        lists, fragments = agc_test_split.migrated(original(), EXECUTION)
        self.assertEqual(sorted(fragments), ["agc_alpha_tests", "agc_beta_tests", "agc_driver_image_tests", "agc_gamma_tests"])
        self.assertEqual(fragments["agc_alpha_tests"], "if(BUILD_TESTING)\n" + ALPHA + "endif()\n")
        self.assertEqual(fragments["agc_beta_tests"], "if(BUILD_TESTING)\n" + BETA + "endif()\n")
        self.assertEqual(fragments["agc_driver_image_tests"],
                         "if(BUILD_TESTING)\n    set_tests_properties(agc_driver_image PROPERTIES TIMEOUT 30)\n"
                         "    set_tests_properties(agc_driver_image PROPERTIES SKIP_RETURN_CODE 77)\nendif()\n")
        self.assertIn("agc_add_default_test(agc_gamma_tests)", fragments["agc_gamma_tests"])
        self.assertNotIn("agc_alpha", lists)
        include = lists.index(agc_test_split.INCLUDE[0])
        self.assertLess(lists.index("agc_add_default_test(${agcTest})"), include)
        self.assertLess(include, lists.index("get_directory_property(agcRegisteredTests TESTS)"))

    def test_split_keeps_a_loop_over_several_executables(self):
        tweaks = ("    foreach(agcTest IN ITEMS agc_alpha_tests agc_gamma_tests)\n"
                  "        set_target_properties(${agcTest} PROPERTIES FOLDER tests)\n"
                  "    endforeach()\n")
        lists, fragments = agc_test_split.migrated(original(tweaks=tweaks), EXECUTION)
        self.assertIn("foreach(agcTest IN ITEMS agc_alpha_tests agc_gamma_tests)", lists)
        self.assertLess(lists.index(agc_test_split.INCLUDE[0]), lists.index("foreach(agcTest IN ITEMS agc_alpha_tests"))

    def test_split_moves_a_loop_over_computed_test_names_with_their_executable(self):
        pattern = ("    foreach(betaCase IN ITEMS one:1 two:2)\n"
                   "        string(REGEX REPLACE \"^([a-z]+):.*$\" \"\\\\1\" betaName \"${betaCase}\")\n"
                   "        set_tests_properties(agc_beta_${betaName} PROPERTIES LABELS beta)\n"
                   "    endforeach()\n")
        lists, fragments = agc_test_split.migrated(original(blocks=(ALPHA, BETA, pattern)), EXECUTION)
        self.assertIn("agc_beta_${betaName}", fragments["agc_beta_tests"])
        self.assertNotIn("agc_beta_${betaName}", lists)

    def test_split_moves_a_loop_that_sets_properties_on_the_tests_it_adds(self):
        state = ("    add_test_executable(agc_beta_state_tests tests/BetaState.cpp)\n"
                 "    set(betaModes three four)\n"
                 "    foreach(mode IN LISTS betaModes)\n"
                 "        add_test(NAME agc_beta_${mode} COMMAND agc_beta_state_tests ${mode})\n"
                 "        set_tests_properties(agc_beta_${mode} PROPERTIES TIMEOUT 9)\n"
                 "    endforeach()\n")
        lists, fragments = agc_test_split.migrated(original(blocks=(ALPHA, BETA, state)), EXECUTION)
        self.assertIn("set_tests_properties(agc_beta_${mode} PROPERTIES TIMEOUT 9)", fragments["agc_beta_state_tests"])
        self.assertNotIn("betaModes", lists)

    def test_split_refuses_a_computed_test_name_several_executables_match(self):
        pattern = ("    foreach(agcCase IN ITEMS alpha beta_one)\n"
                   "        set_tests_properties(agc_${agcCase} PROPERTIES LABELS agc)\n"
                   "    endforeach()\n")
        with self.assertRaises(agc_test_split.SplitError):
            agc_test_split.migrated(original(blocks=(ALPHA, BETA, pattern)), EXECUTION)

    def test_split_refuses_when_no_single_place_fits(self):
        text = original().replace("add_library(libSceAgcDriver SHARED EXCLUDE_FROM_ALL Export.cpp)\n",
                                  "add_library(libSceAgcDriver SHARED EXCLUDE_FROM_ALL Export.cpp)\n"
                                  "add_dependencies(libSceAgcDriver agc_alpha_tests)\n")
        text = text.replace("add_test_executable(agc_alpha_tests tests/Alpha.cpp)",
                            "add_test_executable(agc_alpha_tests tests/Alpha.cpp ${agcLateSource})")
        text = text.replace("if(BUILD_TESTING)\n    get_directory_property",
                            "set(agcLateSource Late.cpp)\nif(BUILD_TESTING)\n    get_directory_property")
        with self.assertRaises(agc_test_split.SplitError):
            agc_test_split.migrated(text, EXECUTION)


class ArgumentTests(unittest.TestCase):
    def test_long_bracket_delimiters(self):
        equals = "=" * 40
        for prefix, expected in (("", ["A", "x y", "tail"]), ("#", ["A", "tail"])):
            with self.subTest(prefix=prefix):
                text = f"set(A {prefix}[{equals}[x y]{equals}] tail)"
                arguments, end, _ = agc_test_split.arguments_of(text, text.index("(") + 1, 0)
                self.assertEqual(arguments, expected)
                self.assertEqual(end, len(text))

    def test_bracket_comment_ends_at_its_closing_bracket(self):
        text = "set(A #[[c]]x)\n"
        arguments, end, _ = agc_test_split.arguments_of(text, text.index("(") + 1, 0)
        self.assertEqual(arguments, ["A", "x"])
        self.assertEqual(end, len(text) - 1)

    def test_unterminated_quoted_and_bracket_arguments_are_split_errors(self):
        for text in ('set(A "unfinished', 'set(A [=[unfinished', 'set(A #[=[unfinished'):
            with self.subTest(text=text), self.assertRaisesRegex(agc_test_split.SplitError, "line 1: unterminated"):
                agc_test_split.commands(text)

    def test_final_line_comment_without_newline_does_not_loop(self):
        code = ("import sys; sys.path.insert(0, %r); "
                "from agc_test_split import SplitError, commands; "
                "\ntry: commands('set(A #comment')"
                "\nexcept SplitError as error: print(error)"
                "\nelse: raise AssertionError('missing SplitError')") % str(TOOL.parent)
        result = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, timeout=2)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("line 1: unterminated", result.stdout)

    def test_bracket_arguments_ignore_only_the_first_immediate_newline(self):
        values = (("\npath", "path"), ("\n\npath", "\npath"), ("\n path", " path"),
                  (" \npath", " \npath"), (r"\npath", r"\npath"), ("\n", ""), ("\npath\n", "path\n"))
        for equals in ("", "=", "==="):
            for content, expected in values:
                with self.subTest(equals=equals, content=content):
                    command = f"set(A [{equals}[{content}]{equals}] tail)"
                    current, following = agc_test_split.commands(command + "\nset(B next)\n")
                    self.assertEqual(current.arguments, ["A", expected, "tail"])
                    self.assertEqual(current.last, command.count("\n"))
                    self.assertEqual(following.first, current.last + 1)
        quoted, = agc_test_split.commands('set(A "\npath" tail)\n')
        self.assertEqual(quoted.arguments, ["A", "\npath", "tail"])


class ResolveTests(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp())
        self.git("init", "-q", "-b", "main")
        self.git("config", "core.safecrlf", "false")
        self.write(LISTS, original())
        self.write(agc_test_split.EXECUTION + "/Image.cpp", "")
        self.commit("build: register tests")

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def git(self, *args, check=True):
        return subprocess.run(["git", "-c", "user.name=Split Test", "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false",
                               "-c", "core.editor=true", *args], cwd=self.root, capture_output=True, text=True, check=check,
                              env={**os.environ, "GIT_EDITOR": "true"})

    def write(self, path, text):
        (self.root / path).parent.mkdir(parents=True, exist_ok=True)
        (self.root / path).write_text(text, encoding="utf-8")

    def read(self, path):
        return (self.root / path).read_text(encoding="utf-8")

    def tool(self, command):
        return subprocess.run([sys.executable, str(TOOL), command], cwd=self.root, capture_output=True, text=True, timeout=60)

    def commit(self, message):
        self.git("add", "-A")
        self.git("commit", "-qm", message)

    def rebase(self, change, after_split=None, delete=()):
        self.git("checkout", "-qb", "change")
        self.write(LISTS, change)
        for path in delete:
            (self.root / path).unlink()
        self.commit("test(agc): change tests")
        self.git("checkout", "-q", "main")
        result = self.tool("split")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.commit("build(agc): split test registrations")
        if after_split:
            after_split()
            self.commit("test(agc): change a fragment")
        self.git("checkout", "-q", "change")
        self.assertNotEqual(self.git("rebase", "main", check=False).returncode, 0)
        return self.tool("resolve")

    def test_resolve_moves_a_new_test_executable_into_its_own_fragment(self):
        result = self.rebase(original(blocks=(ALPHA, BETA, DELTA)))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout.split(), [LISTS, f"{FRAGMENTS}/agc_delta_tests.cmake"])
        self.assertEqual(self.git("diff", "--name-only", "--diff-filter=U").stdout, "")
        self.git("rebase", "--continue")
        self.assertEqual(self.read(f"{FRAGMENTS}/agc_delta_tests.cmake"), "if(BUILD_TESTING)\n" + DELTA + "endif()\n")
        self.assertEqual(self.read(LISTS), self.git("show", "main:" + LISTS).stdout)

    def test_resolve_applies_an_edit_and_a_default_test_to_their_fragments(self):
        change = original(blocks=(ALPHA.replace("TIMEOUT 20", "TIMEOUT 25"), BETA, DELTA), items="agc_gamma_tests agc_delta_tests")
        result = self.rebase(change)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.git("rebase", "--continue")
        self.assertIn("TIMEOUT 25", self.read(f"{FRAGMENTS}/agc_alpha_tests.cmake"))
        self.assertIn("agc_add_default_test(agc_delta_tests)", self.read(f"{FRAGMENTS}/agc_delta_tests.cmake"))
        self.assertEqual(self.read(LISTS), self.git("show", "main:" + LISTS).stdout)

    def test_resolve_merges_with_a_fragment_changed_after_the_split(self):
        def later():
            path = f"{FRAGMENTS}/agc_alpha_tests.cmake"
            self.write(path, self.read(path).replace("PRIVATE libc", "PRIVATE libc libkernel"))

        result = self.rebase(original(blocks=(ALPHA.replace("TIMEOUT 20", "TIMEOUT 25"), BETA)), later)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        alpha = self.read(f"{FRAGMENTS}/agc_alpha_tests.cmake")
        self.assertIn("PRIVATE libc libkernel", alpha)
        self.assertIn("TIMEOUT 25", alpha)

    def test_resolve_leaves_markers_where_both_sides_changed_a_line(self):
        def later():
            path = f"{FRAGMENTS}/agc_alpha_tests.cmake"
            self.write(path, self.read(path).replace("TIMEOUT 20", "TIMEOUT 60"))

        result = self.rebase(original(blocks=(ALPHA.replace("TIMEOUT 20", "TIMEOUT 25"), BETA)), later)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("agc_alpha_tests.cmake", result.stderr)
        self.assertIn("<<<<<<<", self.read(f"{FRAGMENTS}/agc_alpha_tests.cmake"))

    def test_resolve_removes_the_fragment_of_a_deleted_execution_test(self):
        result = self.rebase(original(image=False), delete=[agc_test_split.EXECUTION + "/Image.cpp"])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(f"{FRAGMENTS}/agc_driver_image_tests.cmake", result.stdout.split())
        self.assertEqual(self.git("diff", "--name-only", "--diff-filter=U").stdout, "")
        self.git("rebase", "--continue")
        self.assertFalse((self.root / FRAGMENTS / "agc_driver_image_tests.cmake").exists())
        self.assertNotIn("agc_driver_image", self.read(LISTS))

    def test_resolve_stages_nothing_when_every_file_conflicts(self):
        def later():
            path = f"{FRAGMENTS}/agc_alpha_tests.cmake"
            self.write(path, self.read(path).replace("TIMEOUT 20", "TIMEOUT 60"))
            self.write(LISTS, self.read(LISTS).replace("Export.cpp", "Main.cpp"))

        change = original(blocks=(ALPHA.replace("TIMEOUT 20", "TIMEOUT 25"), BETA), library="Library.cpp")
        result = self.rebase(change, later)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(LISTS, result.stderr)
        self.assertNotEqual(self.git("ls-files", "-u", "--", LISTS).stdout, "")
        self.assertEqual(self.git("diff", "--cached", "--name-only", "--diff-filter=ACDMR").stdout, "")
        self.assertIn("<<<<<<<", self.read(LISTS))

    def test_resolve_needs_a_conflict(self):
        result = self.tool("resolve")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not in a three-way conflict", result.stderr)

    def test_split_refuses_a_second_run(self):
        self.assertEqual(self.tool("split").returncode, 0)
        result = self.tool("split")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("already includes", result.stderr)


class ExecutionTargetTests(unittest.TestCase):
    def test_camel_case_stems_become_snake_case_targets(self):
        self.assertEqual(agc_test_split.execution_target("DrawIndex"), "agc_driver_draw_index_tests")
        self.assertEqual(agc_test_split.execution_target("Dispatch2D"), "agc_driver_dispatch2_d_tests")

    def test_lowercase_and_acronym_stems_are_not_split(self):
        self.assertEqual(agc_test_split.execution_target("blit"), "agc_driver_blit_tests")
        self.assertEqual(agc_test_split.execution_target("ABTest"), "agc_driver_abtest_tests")


class ExecutionTargetsTests(unittest.TestCase):
    def test_cpp_files_map_to_sorted_targets(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            directory = root / agc_test_split.EXECUTION
            directory.mkdir(parents=True)
            for file in ("ZetaOne.cpp", "AlphaTwo.cpp", "notes.txt"):
                (directory / file).write_text("")
            self.assertEqual(agc_test_split.execution_targets(root), ["agc_driver_alpha_two_tests", "agc_driver_zeta_one_tests"])

    def test_empty_directory_has_no_targets(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            (root / agc_test_split.EXECUTION).mkdir(parents=True)
            self.assertEqual(agc_test_split.execution_targets(root), [])

class DedentTests(unittest.TestCase):
    def test_dedent_removes_the_amount_only_where_present(self):
        self.assertEqual(agc_test_split.dedent(["    a", "  b", "c"], 2), ["  a", "b", "c"])


class LoopListTests(unittest.TestCase):
    def test_loop_list_returns_the_items_after_items(self):
        self.assertEqual(agc_test_split.loop_list("FOREACH(ITEMS a b)"), ["a", "b"])


class FindLoopTests(unittest.TestCase):
    def test_find_loop_returns_none_or_refuses_duplicates(self):
        lines = ["x", "y"]
        self.assertIsNone(agc_test_split.find_loop(lines, lambda index, text: False, "marker"))
        with self.assertRaises(agc_test_split.SplitError):
            agc_test_split.find_loop(lines, lambda index, text: True, "marker")
class TokenHelperTests(unittest.TestCase):
    def test_until_returns_the_arguments_before_the_first_keyword(self):
        self.assertEqual(agc_test_split.until(["a", "--", "b"], {"--"}), ["a"])
        self.assertEqual(agc_test_split.until(["a", "b"], {"--"}), ["a", "b"])

    def test_literal_refuses_names_with_variables_or_generator_expressions(self):
        self.assertTrue(agc_test_split.literal("abc"))
        self.assertFalse(agc_test_split.literal("${X}"))
        self.assertFalse(agc_test_split.literal("$<CONFIG>"))

    def test_expand_substitutes_every_listed_value(self):
        self.assertEqual(agc_test_split.expand("lib${X}", {"X": ["a", "b"]}), ["liba", "libb"])
        self.assertEqual(agc_test_split.expand("plain", {}), ["plain"])

    def test_expand_yields_nothing_for_an_unknown_variable(self):
        self.assertEqual(agc_test_split.expand("lib${Y}", {"X": ["a"]}), [])

    def test_leading_walks_back_over_comment_lines_only(self):
        self.assertEqual(agc_test_split.leading(["x", "# a", "# b", "code"], 3), 1)
        self.assertEqual(agc_test_split.leading(["code", "y"], 1), 1)
class GraphHelperTests(unittest.TestCase):
    def test_union_find_joins_sets_transitively(self):
        find, join = agc_test_split.union_find(4)
        join(0, 1)
        join(2, 3)
        self.assertEqual(find(0), find(1))
        self.assertNotEqual(find(0), find(2))
        join(1, 3)
        self.assertEqual(find(0), find(2))

    def test_flatten_yields_each_branch_header_before_its_children(self):
        header = agc_test_split.Command("if", [], 1, 1)
        inner = agc_test_split.Command("set", [], 2, 2)
        other_header = agc_test_split.Command("else", [], 3, 3)
        other = agc_test_split.Command("unset", [], 4, 4)
        end = agc_test_split.Command("endif", [], 5, 5)
        block = agc_test_split.Block(header)
        block.branches = [(header, [inner]), (other_header, [other])]
        block.end = end
        self.assertEqual(list(agc_test_split.flatten(block)), [header, inner, other_header, other, end])
        self.assertEqual(list(agc_test_split.flatten(inner)), [inner])
class ReadSetTests(unittest.TestCase):
    def test_reads_uses_defines_and_accumulates(self):
        class Facts:
            def __init__(self, uses, defines, accumulates):
                self.uses, self.defines, self.accumulates = set(uses), set(defines), set(accumulates)
        self.assertEqual(agc_test_split.reads(Facts({'a'}, set(), set())), {'a'})
        self.assertEqual(agc_test_split.reads(Facts({'a'}, {'a'}, set())), set())
        self.assertEqual(agc_test_split.reads(Facts(set(), {'a'}, {'b'})), {'b'})
class SubjectNameTests(unittest.TestCase):
    class Facts:
        def __init__(self):
            self.subject_targets, self.subject_tests, self.dynamic, self.test_patterns = set(), set(), set(), set()

    def test_subject_records_literal_targets_and_variable_uses(self):
        facts = self.Facts()
        agc_test_split.subject(facts, ['libA', 'lib${X}'], 'target')
        self.assertEqual(facts.subject_targets, {'libA'})
        self.assertEqual(facts.dynamic, {'X'})

    def test_subject_keeps_a_variable_test_name_as_a_pattern(self):
        facts = self.Facts()
        agc_test_split.subject(facts, ['t${Y}'], 'test')
        self.assertEqual(facts.subject_tests, set())
        self.assertEqual(facts.dynamic, {'Y'})
        self.assertEqual(facts.test_patterns, {'t${Y}'})


class CommandErrorTests(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp())

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def tool(self, command):
        return subprocess.run([sys.executable, str(TOOL), command], cwd=self.root, capture_output=True, text=True, timeout=60)

    def test_outside_a_git_checkout_is_a_controlled_error(self):
        result = self.tool("refactor")
        self.assertEqual(result.returncode, 1)
        self.assertIn("not a git repository", result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_missing_registration_file_is_a_controlled_error(self):
        subprocess.run(["git", "init", "-q", "-b", "main"], cwd=self.root, check=True, capture_output=True)
        result = self.tool("refactor")
        self.assertEqual(result.returncode, 1)
        self.assertIn(LISTS, result.stderr)
        self.assertNotIn("Traceback", result.stderr)


if __name__ == "__main__":
    unittest.main()
