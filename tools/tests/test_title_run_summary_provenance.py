import json
import os
import unittest

import test_title_run as fixtures


@unittest.skipUnless(os.name == "posix", "executable script fixture requires POSIX")
class SummaryValidationProvenanceTests(unittest.TestCase):
    setUp = fixtures.NativeProvenanceFlowTests.setUp
    run_fixture = fixtures.NativeProvenanceFlowTests.run_fixture
    assert_qualification = fixtures.NativeProvenanceFlowTests.assert_qualification

    def test_repeated_identity_retains_intervening_commit(self):
        import performance_bisect
        clean = {"commit": self.commit[:12], "dirty": False}
        other = {"commit": "b" * 12, "dirty": False}
        summary, run, path, _ = self.run_fixture([clean, other, clean], {"commit": self.commit, "modified": False})
        self.assertEqual(run["native"]["summary"], [clean, other])
        self.assert_qualification(summary, "conflicting")
        with self.assertRaisesRegex(ValueError, "native.*conflicting"):
            performance_bisect.sample_summary(path, "PPSA00001", self.commit)

    def test_repeated_identity_retains_intervening_numeric_dirty_flag(self):
        import performance_bisect
        clean = {"commit": self.commit[:12], "dirty": False}
        numeric = {"commit": self.commit[:12], "dirty": 0}
        summary, run, path, _ = self.run_fixture([clean, numeric, clean], {"commit": self.commit, "modified": False})
        identities = run["native"]["summary"]
        self.assertEqual(identities, [clean, numeric])
        self.assertIs(identities[0]["dirty"], False)
        self.assertIs(type(identities[1]["dirty"]), int)
        self.assert_qualification(summary, "unknown")
        with self.assertRaisesRegex(ValueError, "native.*unknown"):
            performance_bisect.sample_summary(path, "PPSA00001", self.commit)

    def test_identical_intervals_share_one_identity_and_keep_their_presents(self):
        import performance_bisect
        clean = {"commit": self.commit[:12], "dirty": False}
        summary, run, path, _ = self.run_fixture([clean, clean], {"commit": self.commit, "modified": False})
        self.assertEqual(run["native"]["summary"], [clean])
        self.assertEqual(run["presents"], 120)
        self.assert_qualification(summary, "matched")
        self.assertEqual(performance_bisect.sample_summary(path, "PPSA00001", self.commit)["fps"], 60.0)

    def test_invalid_metrics_cannot_hide_conflicting_native_evidence(self):
        import performance_bisect
        invalid = json.dumps({"format": "aps5-bench/1", "commit": "b" * 12, "dirty": False,
                              "presents": True, "window": {"ms": 1000}})
        summary, run, path, markdown = self.run_fixture([{"commit": self.commit[:12], "dirty": False}],
            {"commit": self.commit, "modified": False}, summary_rows=[invalid])
        self.assert_qualification(summary, "conflicting")
        self.assertEqual(run["fps"], 60.0)
        self.assertEqual(len(run["native"]["summary"]), 2)
        self.assertIn("conflicting", markdown)
        with self.assertRaisesRegex(ValueError, "native.*conflicting"):
            performance_bisect.sample_summary(path, "PPSA00001", self.commit)


    def test_duplicate_protocol_fields_do_not_certify_matching_identity(self):
        import performance_bisect
        duplicate = '{"format":"aps5-bench/1","commit":"bbbbbbbbbbbb","dirty":false,"commit":"' + self.commit[:12] + '"}'
        summary, run, path, markdown = self.run_fixture([{"commit": self.commit[:12], "dirty": False}],
            {"commit": self.commit, "modified": False}, summary_rows=[duplicate])
        self.assert_qualification(summary, "unknown")
        self.assertEqual(run["native"]["summary_errors"], {"malformed_rows": 1})
        self.assertNotIn("bbbbbbbbbbbb", json.dumps(run["native"]))
        self.assertIn("unknown", markdown)
        with self.assertRaisesRegex(ValueError, "native.*unknown"):
            performance_bisect.sample_summary(path, "PPSA00001", self.commit)


    def test_deep_malformed_protocol_row_keeps_valid_metrics_readable(self):
        import performance_bisect
        deep = '{"format":"aps5-bench/1","commit":"bbbbbbbbbbbb","dirty":false,"window":' + "[" * 1100 + "0"
        summary, run, path, markdown = self.run_fixture([{"commit": self.commit[:12], "dirty": False}],
            {"commit": self.commit, "modified": False}, summary_rows=[deep])
        self.assert_qualification(summary, "unknown")
        self.assertEqual(run["native"]["summary_errors"], {"malformed_rows": 1})
        self.assertNotIn("bbbbbbbbbbbb", json.dumps(run["native"]))
        with self.assertRaisesRegex(ValueError, "native.*unknown"):
            performance_bisect.sample_summary(path, "PPSA00001", self.commit)



if __name__ == "__main__":
    unittest.main()
