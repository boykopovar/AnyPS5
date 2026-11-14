import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import progress_diff


def snapshot(libraries=(), shaders=(), library_percent=0, shader_percent=0):
    return {"libraries": {"percent": library_percent, "groups": list(libraries)},
            "shaders": {"percent": shader_percent, "groups": list(shaders)}}


class PresenceTests(unittest.TestCase):
    def test_group_presence_is_added_removed_or_both(self):
        base = snapshot(libraries=[{"name": "kept"}, {"name": "gone"}])
        head = snapshot(libraries=[{"name": "kept"}, {"name": "new"}])
        rows = progress_diff.compare(base, head)["libraries"]["groups"]
        self.assertEqual([(row["name"], row["presence"]) for row in rows],
                         [("gone", "removed"), ("kept", "both"), ("new", "added")])

    def test_group_rows_are_sorted_by_name_across_both_snapshots(self):
        base = snapshot(libraries=[{"name": "c"}, {"name": "a"}])
        head = snapshot(libraries=[{"name": "b"}, {"name": "a"}])
        rows = progress_diff.compare(base, head)["libraries"]["groups"]
        self.assertEqual([row["name"] for row in rows], ["a", "b", "c"])

    def test_sections_are_compared_separately(self):
        base = snapshot(libraries=[{"name": "same"}], shaders=[])
        head = snapshot(libraries=[{"name": "same"}], shaders=[{"name": "same"}])
        result = progress_diff.compare(base, head)
        self.assertEqual(set(result), {"libraries", "shaders"})
        self.assertEqual(result["libraries"]["groups"][0]["presence"], "both")
        self.assertEqual([(row["name"], row["presence"]) for row in result["shaders"]["groups"]], [("same", "added")])

    def test_missing_group_gets_empty_symbol_lists_on_the_absent_side(self):
        base = snapshot(libraries=[{"name": "gone", "done_names": ["a"]}])
        head = snapshot(libraries=[{"name": "new", "todo_names": ["b"]}])
        rows = {row["name"]: row for row in progress_diff.compare(base, head)["libraries"]["groups"]}
        self.assertEqual(rows["gone"]["presence"], "removed")
        self.assertEqual(rows["gone"]["done_names"], {"added": [], "removed": ["a"]})
        self.assertEqual(rows["new"]["presence"], "added")
        self.assertEqual(rows["new"]["todo_names"], {"added": ["b"], "removed": []})


class SymbolSetTests(unittest.TestCase):
    def row(self, base_group, head_group):
        return progress_diff.compare(snapshot(libraries=[base_group]), snapshot(libraries=[head_group]))["libraries"]["groups"][0]

    def test_each_state_reports_added_and_removed_names(self):
        row = self.row({"name": "G", "done_names": ["a", "b"], "todo_names": ["c"], "inert_names": ["d"]},
                       {"name": "G", "done_names": ["b", "c"], "todo_names": ["a", "d"], "inert_names": []})
        self.assertEqual(row["done_names"], {"added": ["c"], "removed": ["a"]})
        self.assertEqual(row["todo_names"], {"added": ["a", "d"], "removed": ["c"]})
        self.assertEqual(row["inert_names"], {"added": [], "removed": ["d"]})

    def test_name_moving_between_states_appears_in_both_states(self):
        row = self.row({"name": "G", "done_names": ["a"]}, {"name": "G", "todo_names": ["a"]})
        self.assertEqual(row["done_names"], {"added": [], "removed": ["a"]})
        self.assertEqual(row["todo_names"], {"added": ["a"], "removed": []})

    def test_symbol_lists_are_sorted(self):
        row = self.row({"name": "G", "todo_names": ["q", "b"]}, {"name": "G", "todo_names": ["m", "a"]})
        self.assertEqual(row["todo_names"], {"added": ["a", "m"], "removed": ["b", "q"]})

    def test_absent_state_key_counts_as_no_symbols(self):
        row = self.row({"name": "G"}, {"name": "G", "inert_names": ["e"]})
        self.assertEqual(row["inert_names"], {"added": ["e"], "removed": []})
        self.assertEqual(row["done_names"], {"added": [], "removed": []})

    def test_duplicate_symbol_names_are_reported_once(self):
        row = self.row({"name": "G"}, {"name": "G", "done_names": ["x", "x"]})
        self.assertEqual(row["done_names"], {"added": ["x"], "removed": []})


class PercentageTests(unittest.TestCase):
    def section(self, base_percent, head_percent):
        return progress_diff.compare(snapshot(library_percent=base_percent), snapshot(library_percent=head_percent))["libraries"]

    def test_change_is_rounded_to_two_decimals(self):
        cases = ((10.0, 10.126, 0.13), (10.0, 9.874, -0.13), (0, 0.124, 0.12), (12.345, 12.349, 0.0))
        for base_percent, head_percent, change in cases:
            with self.subTest(base=base_percent, head=head_percent):
                section = self.section(base_percent, head_percent)
                self.assertEqual(section["change_percentage_points"], change)

    def test_baseline_and_candidate_percent_are_passed_through_unrounded(self):
        section = self.section(12.345, 12.349)
        self.assertEqual(section["baseline_percent"], 12.345)
        self.assertEqual(section["candidate_percent"], 12.349)

    def test_each_section_has_its_own_percentages(self):
        base = snapshot(library_percent=10.0, shader_percent=50)
        head = snapshot(library_percent=10.126, shader_percent=49.5)
        result = progress_diff.compare(base, head)
        self.assertEqual(result["libraries"]["change_percentage_points"], 0.13)
        self.assertEqual(result["shaders"]["change_percentage_points"], -0.5)
        self.assertEqual(result["shaders"]["baseline_percent"], 50)


class MainErrorTests(unittest.TestCase):
    def run_main(self, base_text, head_text):
        with tempfile.TemporaryDirectory() as directory:
            base, head = Path(directory) / "base.json", Path(directory) / "head.json"
            base.write_text(base_text, encoding="utf-8")
            head.write_text(head_text, encoding="utf-8")
            stdout, stderr = io.StringIO(), io.StringIO()
            with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr), self.assertRaises(SystemExit) as raised:
                progress_diff.main([str(base), str(head)])
            return raised.exception.code, stdout.getvalue(), stderr.getvalue()

    def test_invalid_json_is_a_parser_error_with_exit_code_2(self):
        code, stdout, stderr = self.run_main(json.dumps(snapshot()), "{")
        self.assertEqual(code, 2)
        self.assertEqual(stdout, "")
        self.assertIn("error:", stderr)
        self.assertNotIn("Traceback", stderr)


if __name__ == "__main__":
    unittest.main()
