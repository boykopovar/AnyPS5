import json
from itertools import combinations
from pathlib import Path
import sys
import threading
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap


def pull_requests(body):
    response = [{"number": number, "user": {"login": "contributor"}, "head": {"sha": str(number) * 40, "ref": f"branch{number}"},
                 "body": body if number == 1 else "Depends on: none"} for number in (1, 2)]
    with patch.object(pr_overlap, "REPO", "owner/repo", create=True), \
            patch.object(pr_overlap, "gh", return_value=json.dumps(response)):
        return pr_overlap.open_prs("main")


STACK = {
    1: (set(), {"one.cpp"}, {"sceFooInit"}),
    2: ({1, 999}, {"one.cpp", "two.cpp"}, {"sceFooInit"}),
    3: ({2}, {"one.cpp", "two.cpp", "three.cpp"}, {"sceFooInit"}),
    4: ({1}, {"one.cpp", "four.cpp"}, {"sceFooInit", "sceFooTerm"}),
    5: ({1}, {"one.cpp", "four.cpp"}, {"sceFooInit", "sceFooTerm"}),
    6: (set(), {"one.cpp"}, {"sceFooInit"}),
    7: ({8}, {"one.cpp", "seven.cpp"}, {"sceFooInit"}),
    8: ({7, 1}, {"one.cpp", "seven.cpp"}, {"sceFooInit"}),
}


def stacked_pull_requests():
    return {number: {"author": f"author{number}", "depends": depends, "added": added, "exports": exports, "tree": None}
            for number, (depends, added, exports) in STACK.items()}


class DependencyTests(unittest.TestCase):
    def test_documented_form_without_colon(self):
        for body in ("Depends on #2.", "Depends on #2, #3.", "Depends on\t#2 and #3.",
                     "Depends on #2, and #3.", "**Depends on #2.**", "- [x] Depends on #2"):
            with self.subTest(body=body):
                expected = {2, 3} if "#3" in body else {2}
                self.assertEqual(pull_requests(body)[1]["depends"], expected)

    def test_colon_forms_are_preserved(self):
        for body in ("Depends on:#2, #3", "- [x] Depends on: #2, #3", "Depends on:\n- #2\n- #3",
                     "- [x] Depends on:\r\n  * #2\r\n  * #3"):
            with self.subTest(body=body):
                self.assertEqual(pull_requests(body)[1]["depends"], {2, 3})

    def test_markdown_links_and_full_urls(self):
        for body in (
            "- [x] Depends on: [#2](https://github.com/owner/repo/pull/2)",
            "- [x] Depends on: https://github.com/owner/repo/pull/2",
            "- [x] Depends on: [PR #2](https://github.com/owner/repo/pull/2)",
            "- [x] Depends on: https://github.com/owner/repo/pull/2/files",
            "- [x] Depends on: https://github.com/owner/repo/pull/2#issuecomment-1",
            "- [x] Depends on: https://github.com/BOYKOPOVAR/AnyPS5/pull/2",
            "Depends on: [#2](https://github.com/owner/repo/pull/2), [#3](https://github.com/owner/repo/pull/3)",
            "Depends on:\n- [#2](https://github.com/owner/repo/pull/2)\n- [#3](https://github.com/owner/repo/pull/3)",
            "- [x] Depends on: [none](https://example.com)",
            "- [x] Depends on: https://github.com/shadps4-emu/shadPS4/pull/1234",
            "- [x] Depends on: [#1234](https://github.com/shadps4-emu/shadPS4/pull/1234)",
        ):
            with self.subTest(body=body):
                expected = set() if "none" in body or "shadps4" in body else ({2, 3} if "3" in body else {2})
                self.assertEqual(pull_requests(body)[1]["depends"], expected)

    def test_later_references_are_not_dependencies(self):
        for newline in ("\n", "\r\n"):
            for declaration in ("Depends on #2.", "Depends on: #2"):
                body = newline.join((declaration, "- #3", "", "Related work: #4.", "- #5"))
                with self.subTest(body=body):
                    expected = {2, 3} if ":" in declaration else {2}
                    self.assertEqual(pull_requests(body)[1]["depends"], expected)

    def test_no_dependencies(self):
        for body in (None, "", "Depends on: none", "Depends on: <!-- #PR, or none -->",
                     "Related to #2.", "Depends on compiler support; see #2.",
                     "Depends on\n#2", "Depends on #2suffix"):
            with self.subTest(body=body):
                self.assertEqual(pull_requests(body)[1]["depends"], set())

    def test_plain_declaration_stops_before_unrelated_prose(self):
        for body in ("Depends on #2. Related #3 covers something else.",
                     "Depends on #2; related #3 covers something else.",
                     "Depends on #2 and compiler support from #3.",
                     "Depends on #2, related work is in #3.",
                     "**Depends on #2**. Refs #3."):
            with self.subTest(body=body):
                self.assertEqual(pull_requests(body)[1]["depends"], {2})

    def test_colon_declaration_stops_before_unrelated_prose(self):
        for body in ("Depends on: #2. Related #3 covers something else.",
                     "Depends on: #2; related #3 covers something else.",
                     "Depends on: none; see issue #3.",
                     "Depends on: #2, related work is in #3."):
            with self.subTest(body=body):
                expected = set() if "none" in body else {2}
                self.assertEqual(pull_requests(body)[1]["depends"], expected)

    def test_checklist_declaration_keeps_precedence(self):
        for declaration, expected in (("#3", {3}), ("none", set())):
            body = "Previously: Depends on #2.\n\n- [x] Depends on: " + declaration
            with self.subTest(body=body):
                self.assertEqual(pull_requests(body)[1]["depends"], expected)

    def test_template_placeholder_does_not_hide_prose(self):
        placeholder = "- [ ] Depends on: <!-- #PR, or none -->"
        for prose, expected in (("Adds X. Depends on #2.", {2}), ("Adds X.", set()), ("Depends on #2, #3.", {2, 3})):
            for newline in ("\n", "\r\n"):
                body = newline.join((prose, placeholder))
                with self.subTest(body=body):
                    self.assertEqual(pull_requests(body)[1]["depends"], expected)

    def test_empty_colon_declaration_does_not_hide_a_later_one(self):
        for body in ("- [ ] Depends on: <!-- #PR, or none -->\n\nDepends on: #2, #3",
                     "Depends on:\n\nDepends on: #2 and #3"):
            with self.subTest(body=body):
                self.assertEqual(pull_requests(body)[1]["depends"], {2, 3})

    def test_checklist_none_wins_over_prose_with_placeholder_comment(self):
        body = "Depends on #2.\n- [x] Depends on: none <!-- #PR, or none -->"
        self.assertEqual(pull_requests(body)[1]["depends"], set())

    def test_documented_dependency_suppresses_overlap(self):
        prs = pull_requests("Depends on #2.")
        for pr in prs.values():
            pr["added"] = {"shared.cpp"}
        self.assertIsNone(pr_overlap.overlaps(prs, 1, 2))
        self.assertIsNone(pr_overlap.overlaps(prs, 2, 1))

    def test_reported_link_round_trips_as_a_dependency(self):
        with patch.object(pr_overlap, "REPO", "owner/repo", create=True):
            link = pr_overlap.pull("https://github.com/owner/repo/blob/abc123", 2)
        self.assertIn("https://www.github.com/owner/repo/pull/2", link)
        prs = pull_requests(f"Depends on: {link}")
        self.assertEqual(prs[1]["depends"], {2})
        for pr in prs.values():
            pr["added"] = {"shared.cpp"}
        self.assertIsNone(pr_overlap.overlaps(prs, 1, 2))

    def test_www_github_dependencies_are_supported(self):
        for reference in ("https://www.github.com/owner/repo/pull/2",
                          "[PR 2](https://www.github.com/owner/repo/pull/2)"):
            with self.subTest(reference=reference):
                self.assertEqual(pull_requests(f"Depends on: {reference}")[1]["depends"], {2})

    def test_unrelated_reference_keeps_overlap(self):
        prs = pull_requests("Related to #2.")
        for pr in prs.values():
            pr["added"] = {"shared.cpp"}
        self.assertEqual(pr_overlap.overlaps(prs, 1, 2), {"added": {"shared.cpp"}})


class StackedDependencyTests(unittest.TestCase):
    def test_transitive_dependency_suppresses_overlap(self):
        prs = stacked_pull_requests()
        self.assertIsNone(pr_overlap.overlaps(prs, 1, 3))
        self.assertIsNone(pr_overlap.overlaps(prs, 3, 1))

    def test_changes_inherited_from_a_shared_dependency_are_not_overlap(self):
        prs = stacked_pull_requests()
        self.assertIsNone(pr_overlap.overlaps(prs, 3, 4))
        self.assertIsNone(pr_overlap.overlaps(prs, 2, 4))

    def test_overlap_beyond_a_shared_dependency_is_kept(self):
        self.assertEqual(pr_overlap.overlaps(stacked_pull_requests(), 4, 5),
                         {"added": {"four.cpp"}, "conflicts": {}, "exports": {"sceFooTerm"}})

    def test_independent_pull_request_keeps_inherited_overlap(self):
        self.assertEqual(pr_overlap.overlaps(stacked_pull_requests(), 3, 6),
                         {"added": {"one.cpp"}, "conflicts": {}, "exports": {"sceFooInit"}})

    def test_dependency_cycle_terminates(self):
        prs = stacked_pull_requests()
        self.assertIsNone(pr_overlap.overlaps(prs, 7, 8))
        self.assertIsNone(pr_overlap.overlaps(prs, 8, 2))


class PairComparisonTests(unittest.TestCase):
    def test_pairs_that_change_the_same_files_are_compared_on_the_workers(self):
        prs = {n: {"author": f"author{n % 3}", "files": {"shared.txt"} if n % 2 else {f"own{n}.txt"}} for n in range(1, 61)}
        threads, lines = {}, []

        def fake_overlaps(prs, a, b):
            threads[a, b] = threading.current_thread()
            return {"added": {f"{a}-{b}"}} if (a + b) % 7 == 0 else None

        with patch.object(pr_overlap, "overlaps", side_effect=fake_overlaps), patch.object(pr_overlap, "progress", side_effect=lines.append):
            serial = {n: {} for n in prs}
            for a, b in combinations(prs, 2):
                if found := pr_overlap.overlaps(prs, a, b):
                    serial[a][b] = serial[b][a] = found
            threads.clear()
            rows = pr_overlap.compare_all(prs, combinations(prs, 2), 4)
        self.assertEqual(rows, serial)
        merging = {(a, b) for a, b in combinations(prs, 2) if a % 2 and b % 2 and a % 3 != b % 3}
        self.assertEqual(len(threads), len(list(combinations(prs, 2))))
        self.assertEqual({pair for pair, thread in threads.items() if thread is not threading.main_thread()}, merging)
        self.assertEqual(lines, [f"compared {len(merging)}/{len(merging)} pull request pairs that change the same files"])

    def test_a_failed_pair_comparison_cancels_the_queued_ones(self):
        prs = {n: {"author": f"author{n}", "files": {"shared.txt"}} for n in range(1, 41)}
        calls = []

        def fake_overlaps(prs, a, b):
            calls.append((a, b))
            if len(calls) == 1:
                raise RuntimeError("git merge-tree failed")
            time.sleep(0.01)

        with patch.object(pr_overlap, "overlaps", side_effect=fake_overlaps), patch.object(pr_overlap, "progress"):
            with self.assertRaisesRegex(RuntimeError, "git merge-tree failed"):
                pr_overlap.compare_all(prs, combinations(prs, 2), 1)
        self.assertLess(len(calls), 10)


if __name__ == "__main__":
    unittest.main()
