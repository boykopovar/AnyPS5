import concurrent.futures
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import performance_bisect
import title_run

A = {"os": "Fixture OS", "gpu": "Fixture GPU A", "driver": "Fixture driver A", "cpu": "Fixture CPU A"}
B = {"os": "Fixture OS", "gpu": "Fixture GPU B", "driver": "Fixture driver B", "cpu": "Fixture CPU B"}
C = {"os": "Fixture OS", "gpu": "Fixture GPU C", "driver": "Fixture driver C", "cpu": "Fixture CPU C"}
GOOD = "1" * 40


def recorded(machines):
    return {"build": {"commit": GOOD, "dirty": False, "note": ""}, "machine": dict(A), "driver_cache": "isolated",
            "runs": len(machines), "seconds": 1,
            "titles": {"PPSA00001": {"name": "Fixture", "version": "1", "seconds": 1, "missing_content": [],
                "runs": [{"run": index + 1, "cache": "cold" if index == 0 else "warm", "outcome": "rendering",
                          "fps": 60.0, "machine": copy.deepcopy(machine), "first_error": "",
                          "native": {"summary": [{"commit": GOOD[:12], "dirty": False}]}}
                         for index, machine in enumerate(machines)]}}}


class RunMachineContextTests(unittest.TestCase):
    def test_all_ordered_run_machines_are_qualified_independently_of_global_fields(self):
        before = recorded([A, B])
        same = copy.deepcopy(before)
        same["machine"] = {field: "unrelated batch aggregate" for field in A}
        self.assertEqual(title_run.comparison_context(same, before, "PPSA00001")["status"], "matched")
        for index in (0, 1):
            for field in A:
                now = copy.deepcopy(before)
                now["titles"]["PPSA00001"]["runs"][index]["machine"][field] = "changed " + field
                with self.subTest(index=index, field=field):
                    context = title_run.comparison_context(now, before, "PPSA00001")
                    self.assertEqual(context["status"], "differing")
                    self.assertIn("runMachines", context["differences"])
                    self.assertEqual(context["unknowns"], {"before": [], "now": []})
                    self.assertEqual(context["now"]["runMachines"][index][field], "changed " + field)
                    self.assertEqual(context["now"][field], now["titles"]["PPSA00001"]["runs"][-1]["machine"][field])
        swapped = recorded([B, A])
        self.assertEqual(title_run.comparison_context(swapped, before, "PPSA00001")["status"], "differing")

    def test_missing_or_malformed_run_fields_are_unknown_without_a_global_fallback(self):
        for index in (0, 1):
            for field in A:
                for missing in (None, "", " ", 7, False, []):
                    before, now = recorded([A, B]), recorded([A, B])
                    now["titles"]["PPSA00001"]["runs"][index]["machine"][field] = missing
                    with self.subTest(index=index, field=field, missing=missing):
                        context = title_run.comparison_context(now, before, "PPSA00001")
                        self.assertEqual(context["status"], "unknown")
                        self.assertIn("runMachines", context["unknowns"]["now"])
                        self.assertIsNone(context["now"]["runMachines"][index][field])
        for machine in (None, [], "machine", {}, {"gpu": "only one field"}):
            now, before = recorded([A, B]), recorded([A, B])
            now["titles"]["PPSA00001"]["runs"][0]["machine"] = machine
            with self.subTest(machine=machine):
                self.assertEqual(title_run.comparison_context(now, before, "PPSA00001")["status"], "unknown")
        legacy = recorded([A, B])
        for run in legacy["titles"]["PPSA00001"]["runs"]:
            del run["machine"]
        self.assertEqual(title_run.comparison_context(legacy, legacy, "PPSA00001")["status"], "unknown")
        self.assertEqual(title_run.compare(legacy, legacy), ([], []))
        markdown = title_run.render_markdown(legacy, legacy)
        self.assertIn("unknown", markdown)
        self.assertIn("exploratory", markdown)


class BisectRunMachineTests(unittest.TestCase):
    def sample(self, document):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            return performance_bisect.sample_summary(path, "PPSA00001", GOOD)

    def test_per_run_machine_is_authoritative_even_without_a_batch_aggregate(self):
        summary = recorded([B])
        self.assertEqual(self.sample(summary)["identity"]["gpu"], "Fixture GPU B")
        del summary["machine"]
        self.assertEqual(self.sample(summary)["identity"]["cpu"], "Fixture CPU B")
        summary["titles"]["PPSA00001"]["runs"][0]["machine"]["cpu"] = ""
        self.assertEqual(self.sample(summary)["identity"]["cpu"], "")

    def test_bad_present_run_machine_never_falls_back_to_a_known_global_record(self):
        for machine in (None, [], "machine", {}, {"gpu": "B"}, {**B, "gpu": ""}, {**B, "driver": 9},
                        {**B, "gpu": " "}, {**B, "os": "\t"}):
            summary = recorded([B])
            summary["titles"]["PPSA00001"]["runs"][0]["machine"] = machine
            with self.subTest(machine=machine), self.assertRaisesRegex(ValueError, "machine"):
                self.sample(summary)
        summary = recorded([B])
        del summary["titles"]["PPSA00001"]["runs"][0]["machine"]["cpu"]
        with self.assertRaisesRegex(ValueError, "machine"):
            self.sample(summary)

    def test_legacy_machine_requires_one_title_and_one_run_in_the_actual_file(self):
        summary = recorded([B])
        del summary["titles"]["PPSA00001"]["runs"][0]["machine"]
        self.assertEqual(self.sample(summary)["identity"]["gpu"], "Fixture GPU A")
        summary["titles"]["PPSA00002"] = {"name": "Another", "runs": []}
        with self.assertRaisesRegex(ValueError, "machine"):
            self.sample(summary)
        del summary["titles"]["PPSA00002"]
        summary["titles"]["PPSA00001"]["runs"].append(copy.deepcopy(summary["titles"]["PPSA00001"]["runs"][0]))
        with self.assertRaisesRegex(ValueError, "exactly one"):
            self.sample(summary)


RUNTIME = '''import json, os, struct, time
from pathlib import Path
counter = Path("fixture-run-count")
index = int(counter.read_text()) if counter.exists() else 0
counter.write_text(str(index + 1))
machine = json.loads(os.environ["FIXTURE_MACHINES"])[index]
row = dict(machine, commit=os.environ["FIXTURE_COMMIT"][:12], dirty=False, interval_start_s=0,
           interval_end_s=1, presents=60, window={"ms": 1000})
Path(os.environ["APS5_TRACE_DIR"], "aps5-summary.jsonl").write_text(json.dumps(row) + "\\n", encoding="utf-8")
Path("anyps5-report.json").write_text(json.dumps({"format": "anyps5-report", "anyps5": {
    "commit": os.environ["FIXTURE_COMMIT"], "modified": False}, "run": {"averageFps": 60}}), encoding="utf-8")
print("host CPU " + machine["cpu"] + ": has fixture features; lacks none", flush=True)
pixels = bytes([0, 0, 0, 255, 255, 255, 255, 255])
Path("frame_1.bmp").write_bytes(struct.pack("<2sIHHI", b"BM", 62, 0, 0, 54) +
    struct.pack("<IiiHHIIiiII", 40, 2, -1, 1, 32, 0, 8, 0, 0, 0, 0) + pixels)
time.sleep(60)
'''


@unittest.skipUnless(os.name == "posix", "executable script fixture requires POSIX")
class MachineCliTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        source = self.root / "source"
        subprocess.run(["git", "init", "-q", str(source)], check=True, capture_output=True)
        subprocess.run(["git", "-C", str(source), "-c", "user.name=Fixture", "-c", "user.email=fixture@invalid",
                        "-c", "commit.gpgsign=false", "-c", f"core.hooksPath={os.devnull}", "commit", "-q",
                        "--allow-empty", "-m", "test: machine fixture"], check=True, capture_output=True)
        self.commit = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
        self.build = self.root / "build"
        converter = self.build / "core/relinker/relinker"
        converter.parent.mkdir(parents=True)
        (self.build / "core/libs/libs").mkdir(parents=True)
        for name in ("libmimalloc.so", "libmimalloc.dylib"):
            (self.build / "core/libs/libs" / name).write_bytes(name.encode("utf-8"))
        (self.build / "CMakeCache.txt").write_text(f"CMAKE_HOME_DIRECTORY:INTERNAL={source}\n", encoding="utf-8")
        converter.with_name("runtime.py").write_text("#!" + sys.executable + "\n" + RUNTIME, encoding="utf-8")
        converter.write_text("#!" + sys.executable + "\nimport pathlib, shutil, sys\n"
                             "p = pathlib.Path(sys.argv[-1])\n"
                             "shutil.copyfile(pathlib.Path(__file__).with_name('runtime.py'), p)\np.chmod(0o700)\n", encoding="utf-8")
        converter.chmod(0o700)
        for title_id in ("PPSA00001", "PPSA00002"):
            package = self.root / "games" / title_id
            (package / "sce_sys").mkdir(parents=True)
            (package / "eboot.bin").write_bytes(b"\x7fELF" + bytes(60))
            (package / "sce_sys/param.json").write_text(json.dumps({"titleId": title_id, "titleName": title_id,
                                                                   "contentVersion": "1"}), encoding="utf-8")

    def run_catalog(self, name, second, previous=None):
        root = self.root / name
        root.mkdir()
        catalog = root / "titles.json"
        catalog.write_text(json.dumps({title_id: {"env": {"FIXTURE_COMMIT": self.commit,
            "FIXTURE_MACHINES": json.dumps(machines)}} for title_id, machines in
            (("PPSA00001", [A, A]), ("PPSA00002", second))}), encoding="utf-8")
        command = [sys.executable, str(Path(title_run.__file__)), "--games", str(self.root / "games"), "--build",
                   str(self.build), "--titles", str(catalog), "--out", str(root / "out"), "--runs", "2", "--seconds", "1"]
        if previous:
            command += ["--compare", str(previous), "--json"]
        done = subprocess.run(command, capture_output=True, text=True, timeout=15)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        paths = list((root / "out").glob("*/summary.json"))
        self.assertEqual(len(paths), 1, done.stdout)
        return paths[0], json.loads(paths[0].read_text(encoding="utf-8")), json.loads(done.stdout) if previous else None

    def test_real_cli_preserves_each_machine_and_qualifies_all_runs(self):
        before_path, before, _ = self.run_catalog("before", [B, B])
        unknown = {field: "" for field in A}
        cases = [("changed", [C, C], "differing"), ("cold-only", [C, B], "differing"),
                 ("same", [B, B], "matched"), ("missing", [unknown, unknown], "unknown"),
                 ("uniform", [A, A], "differing")]
        with concurrent.futures.ThreadPoolExecutor(max_workers=len(cases)) as pool:
            results = list(pool.map(lambda case: self.run_catalog(case[0], case[1], before_path), cases))
        for title_id, machine in (("PPSA00001", A), ("PPSA00002", B)):
            for observed in before["titles"][title_id]["runs"]:
                self.assertEqual(observed.get("machine"), machine)
        for (name, machines, status), (path, now, comparison) in zip(cases, results):
            with self.subTest(name=name):
                self.assertEqual([run.get("machine") for run in now["titles"]["PPSA00002"]["runs"]], machines)
                expected_aggregate = A if name == "uniform" else {field: "Fixture OS" if field == "os" and name != "missing" else "" for field in A}
                self.assertEqual(now["machine"], expected_aggregate)
                first, second = comparison["titles"]["PPSA00001"], comparison["titles"]["PPSA00002"]
                self.assertEqual(first["context"]["status"], "matched")
                self.assertEqual(second["context"]["status"], status)
                self.assertEqual(second["provenance"]["now"]["status"], "matched")
                self.assertEqual(second["fps"], {"before": 60.0, "now": 60.0})
                self.assertEqual(second["context"]["before"]["runMachines"], [B, B])
                if name == "cold-only":
                    self.assertEqual(second["context"]["now"]["gpu"], "Fixture GPU B")
                    self.assertEqual(second["context"]["now"]["runMachines"], [C, B])
                markdown = path.with_name("summary.md").read_text(encoding="utf-8")
                if name != "uniform":
                    self.assertNotIn("Fixture GPU A", markdown.splitlines()[0])
                    self.assertIn("Per-run machine", markdown)
                    self.assertIn("Fixture GPU A", markdown)
                    self.assertIn("not recorded" if name == "missing" else machines[0]["gpu"], markdown)
                else:
                    self.assertIn("Fixture GPU A", markdown.splitlines()[0])


if __name__ == "__main__":
    unittest.main()
