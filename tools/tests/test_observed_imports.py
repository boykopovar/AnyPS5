import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import nid_names
import observed_imports
import progress

FIRST, SECOND, THIRD = "AaAaAaAaAaA", "BbBbBbBbBbB", "CcCcCcCcCcC"


def record(nid, providers, kind="stub", **extra):
    return {"nid": nid, "library": providers[0] if providers else None, "references": 1, "weak": False,
            "class": kind, "providers": providers, "symbol_mismatch": False, **extra}


class ObservedImportsTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.prx = self.root / "prx"
        for name in ("libSceOne", "libSceTwo"):
            (self.prx / name).mkdir(parents=True)
        self.observed = self.root / "observed"

    def report(self, records, name="report.json"):
        path = self.root / name
        path.write_text(json.dumps({"imports": records}), encoding="utf-8")
        return str(path)

    def run_tool(self, *arguments):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = observed_imports.main(["--prx", str(self.prx), "--observed", str(self.observed), *arguments])
        return code, out.getvalue(), err.getvalue()

    def text(self, library):
        return (self.observed / f"{library}.txt").read_text(encoding="utf-8")

    def test_add_writes_the_imported_nids_of_each_providing_library_sorted(self):
        report = self.report([record(SECOND, ["libSceOne.prx"]), record(FIRST, ["libSceOne.prx"], "implemented"),
                              record(THIRD, ["libSceTwo.prx"], "partial")])
        code, out, _ = self.run_tool("add", report)
        self.assertEqual(code, 0, out)
        self.assertEqual(self.text("libSceOne").splitlines()[1:], [FIRST, SECOND])
        self.assertEqual(self.text("libSceTwo").splitlines()[1:], [THIRD])
        self.assertEqual(progress.read_manifest(self.observed / "libSceOne.txt"), frozenset({FIRST, SECOND}))

    def test_add_ignores_imports_no_library_provides(self):
        report = self.report([record(FIRST, [], "absent"), record(SECOND, [], "module")])
        code, out, _ = self.run_tool("add", report)
        self.assertEqual(code, 0)
        self.assertFalse(self.observed.exists())
        self.assertIn("already list", out)

    def test_add_merges_reports_and_keeps_comments_and_the_header(self):
        self.observed.mkdir()
        (self.observed / "libSceOne.txt").write_text(f"# my header\n{THIRD}  # evidence: resolved by name\n", encoding="utf-8")
        report = self.report([record(FIRST, ["libSceOne.prx"])])
        other = self.report([record(FIRST, ["libSceOne.prx"]), record(THIRD, ["libSceOne.prx"])], "other.json")
        self.assertEqual(self.run_tool("add", report, other)[0], 0)
        self.assertEqual(self.text("libSceOne"), f"# my header\n{FIRST}\n{THIRD}  # evidence: resolved by name\n")

    def test_add_is_idempotent(self):
        report = self.report([record(FIRST, ["libSceOne.prx"])])
        self.run_tool("add", report)
        before = self.text("libSceOne")
        code, out, _ = self.run_tool("add", report)
        self.assertEqual((code, self.text("libSceOne")), (0, before))
        self.assertIn("already list", out)

    def test_add_resolves_a_symbol_name_to_its_nid(self):
        report = self.report([record("sceFoo", ["libSceOne.prx"], symbol_mismatch=True)])
        self.assertEqual(self.run_tool("add", report)[0], 0)
        self.assertEqual(self.text("libSceOne").splitlines()[1:], [nid_names.compute_nid("sceFoo")])

    def test_add_rejects_a_library_that_does_not_exist_and_writes_nothing(self):
        report = self.report([record(FIRST, ["libSceOne.prx"]), record(SECOND, ["libSceMissing.prx"])])
        code, _, err = self.run_tool("add", report)
        self.assertEqual(code, 2)
        self.assertIn("libSceMissing", err)
        self.assertFalse(self.observed.exists())

    def test_add_rejects_a_malformed_nid_and_a_malformed_report(self):
        code, _, err = self.run_tool("add", self.report([record("not a nid!", ["libSceOne.prx"])]))
        self.assertEqual(code, 2)
        self.assertIn("malformed NID", err)
        broken = self.root / "broken.json"
        broken.write_text("{", encoding="utf-8")
        self.assertEqual(self.run_tool("add", str(broken))[0], 2)
        broken.write_text('{"imports": 3}', encoding="utf-8")
        self.assertEqual(self.run_tool("add", str(broken))[0], 2)
        self.assertEqual(self.run_tool("add", str(self.root / "absent.json"))[0], 2)
        self.assertFalse(self.observed.exists())

    def test_add_check_reports_without_writing(self):
        report = self.report([record(FIRST, ["libSceOne.prx"])])
        code, out, _ = self.run_tool("add", "--check", report)
        self.assertEqual(code, 1)
        self.assertIn("would add 1 NIDs", out)
        self.assertFalse(self.observed.exists())
        self.run_tool("add", report)
        self.assertEqual(self.run_tool("add", "--check", report)[0], 0)

    def test_entry_records_evidence_and_rejects_bad_input(self):
        code, _, _ = self.run_tool("entry", "libSceOne", FIRST, "--evidence", "resolved by\nsceKernelDlsym")
        self.assertEqual(code, 0)
        self.assertEqual(self.text("libSceOne").splitlines()[1:], [f"{FIRST}  # evidence: resolved by sceKernelDlsym"])
        self.assertEqual(self.run_tool("entry", "libSceOne", "short", "--evidence", "x")[0], 2)
        self.assertEqual(self.run_tool("entry", "libSceNope", SECOND, "--evidence", "x")[0], 2)
        self.assertEqual(self.run_tool("entry", "libSceOne", SECOND, "--evidence", "  ")[0], 2)

    def test_check_accepts_valid_manifests(self):
        self.run_tool("add", self.report([record(FIRST, ["libSceOne.prx"])]))
        code, out, _ = self.run_tool("check")
        self.assertEqual(code, 0)
        self.assertIn("valid", out)

    def test_check_accepts_a_missing_directory(self):
        self.assertEqual(self.run_tool("check")[0], 0)

    def test_check_names_each_problem(self):
        self.observed.mkdir()
        (self.observed / "libSceOne.txt").write_text(f"{SECOND}\n{FIRST}\n{FIRST}\nshort\n", encoding="utf-8")
        (self.observed / "libSceGone.txt").write_text(f"{FIRST}\n", encoding="utf-8")
        (self.observed / "notes.md").write_text("x\n", encoding="utf-8")
        code, out, err = self.run_tool("check")
        self.assertEqual(code, 1)
        for expected in (f"libSceOne.txt:2: {FIRST} is not in sorted order", "libSceOne.txt:3: duplicate", "libSceOne.txt:4: invalid NID 'short'",
                         "libSceGone is not a library", "notes.md: not a <library>.txt file"):
            self.assertIn(expected, err)
        self.assertIn("5 problems", out)


if __name__ == "__main__":
    unittest.main()
