import contextlib
import copy
import hashlib
import io
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import unittest
import urllib.request
from collections import Counter
from pathlib import Path
from unittest.mock import patch

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import nid_names


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "import_census.py"
CLASSES = ("implemented", "partial", "stub", "absent", "module")
LIBRARY = "libExample.prx"
ALPHA_NAME = "sceExampleAlpha"
BETA_NAME = "sceExampleBeta"
ALPHA = nid_names.compute_nid(ALPHA_NAME)
BETA = nid_names.compute_nid(BETA_NAME)


def row(symbol=ALPHA, name=ALPHA_NAME, library=LIBRARY, kind="partial", providers=None, **extra):
    return {"nid": symbol, "name": name, "library": library, "class": kind,
            "providers": [LIBRARY] if providers is None else list(providers),
            "references": 1, "weak": False, "library_mismatch": False,
            "symbol_mismatch": False, "unsearched_providers": [], **extra}


def audit(rows, **extra):
    classes = Counter(entry["class"] for entry in rows)
    references = {kind: sum(entry["references"] for entry in rows if entry["class"] == kind) for kind in CLASSES}
    unsearched = Counter(provider for entry in rows for provider in entry["unsearched_providers"])
    return {"source_commit": "a" * 40, "source_dirty": True,
            "registries": ["C:\\private\\deleted\\eboot.registry.json"], "dependency_registries": [],
            "needed_libraries": [], "references": sum(references.values()), "unique_imports": len(rows),
            "unique_by_class": {kind: classes[kind] for kind in CLASSES}, "references_by_class": references,
            "missing_libraries": {}, "missing_host_dependencies": [],
            "library_mismatch": sum(entry["library_mismatch"] for entry in rows),
            "symbol_mismatch": sum(entry["symbol_mismatch"] for entry in rows),
            "unsearched_providers": dict(unsearched),
            "stops": [{"registry": "C:\\private\\deleted\\eboot.registry.json", "nid": None, "library": None, "name": None}],
            "imports": rows, **extra}


def runtime(title_id="PPSA00001", run_id="run-one", functions=None, fatal=False):
    document = json.loads((ROOT / "docs/run-report-fixtures/stop-missing-function.json").read_text(encoding="utf-8"))
    document["game"].update(titleId=title_id, contentId=None, contentVersion="01.000.000", name="Example")
    document["anyps5"].update(commit="b" * 40, modified=None)
    document["missing"]["functions"] = functions if functions is not None else [
        {"library": "libExample", "name": ALPHA_NAME, "calls": 1}]
    document["run"].update(id=hashlib.sha256(run_id.encode("utf-8")).hexdigest()[:32], recovered=False, incomplete=False)
    if fatal:
        document["stop"].update(kind="missing-function", reason="runtime-stop",
                                message=f"Called libExample {ALPHA_NAME}, which is not implemented.")
    else:
        document["run"].update(exit="clean", exitCode=0)
        document["stop"].update(kind="none", reason="guest-exit", message="The guest process exited.", source=None)
    return document


class ImportCensusTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def write(self, name, document):
        path = self.directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(document, ensure_ascii=False, sort_keys=True, indent=2) + "\n", encoding="utf-8")
        return path

    def digest(self, path):
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()

    def title(self, title_id="PPSA00001", audit_path=None, runtime_paths=(), **extra):
        entry = {"title_id": title_id, "guest_platform": "ps5", **extra}
        if audit_path is not None:
            entry["audit"] = {"path": Path(audit_path).name}
        if runtime_paths:
            entry["runtime_reports"] = [{"path": Path(path).name} for path in runtime_paths]
        return entry

    def manifest(self, *titles, name="corpus.json"):
        return self.write(name, {"format": "anyps5-import-corpus", "version": 1, "titles": list(titles)})

    def cli(self, *arguments, expected=0, environment=None):
        result = subprocess.run([sys.executable, str(SCRIPT), *(str(argument) for argument in arguments)],
                                cwd=self.directory, capture_output=True, text=True, encoding="utf-8", timeout=10,
                                env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1", **(environment or {})})
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        return result

    def build(self, *manifests, extra=()):
        return json.loads(self.cli("build", *manifests, "--format", "json", *extra).stdout)

    def symbol(self, census, symbol=ALPHA):
        return next(entry for entry in census["symbols"] if entry["symbol"] == symbol)

    def test_compact_legacy_manifest_keeps_unknown_provenance_and_exact_evidence(self):
        path = self.write("audit.json", audit([row()], source_dirty=None))
        result = self.build(self.manifest(self.title(audit_path=path)))
        self.assertEqual((result["format"], result["version"], result["status"]), ("anyps5-import-census", 1, "complete"))
        self.assertEqual(result["coverage"]["audited_titles"], 1)
        title = result["titles"][0]
        self.assertEqual(title["key"], "ps5:PPSA00001")
        self.assertIsNone(title["audit"]["source_dirty"])
        for field in ("producer", "counting_policy", "provider_build"):
            self.assertIsNone(title["audit"][field])
        self.assertEqual(title["runtime_coverage"], "unavailable")
        summary = self.symbol(result)
        self.assertEqual(summary["imported_titles"], ["ps5:PPSA00001"])
        self.assertEqual(summary["gap_title_count"], 1)
        self.assertIsNone(summary["observed_call_title_count"])
        self.assertIsNone(summary["native_stop_title_count"])
        self.assertIsNone(summary["manual_stop_title_count"])
        self.assertEqual(summary["current_status"], "unverified")
        observation = result["observations"][0]
        self.assertEqual(observation["evidence"]["sha256"], self.digest(path))
        self.assertEqual(observation["evidence"]["pointer"], "/imports/0")
        self.assertTrue(observation["next_actions"])
        self.assertTrue(all(isinstance(action["kind"], str) for action in observation["next_actions"]))
        encoded = json.dumps(result)
        self.assertNotIn("C:\\\\private", encoded)
        self.assertNotIn(str(self.directory), encoded)

    def test_title_sets_do_not_count_module_rows_copies_or_repeated_manifests(self):
        first = self.write("first.json", audit([row(), row(library=None, module="engine.prx")]))
        second = self.write("second.json", audit([row(references=7)]))
        copied = self.directory / "copied.json"
        shutil.copyfile(first, copied)
        one = self.manifest(self.title(audit_path=first), name="one.json")
        duplicate = self.manifest(self.title(audit_path=copied), name="copy.json")
        two = self.manifest(self.title("PPSA00002", second), name="two.json")
        result = self.build(one, duplicate, two, one)
        summary = self.symbol(result)
        self.assertEqual(summary["imported_titles"], ["ps5:PPSA00001", "ps5:PPSA00002"])
        self.assertEqual(summary["imported_title_count"], 2)
        self.assertEqual(summary["gap_title_count"], 2)
        observations = [entry for entry in result["observations"] if entry["kind"] == "import_presence"]
        self.assertEqual(len(observations), 3)
        self.assertEqual(sorted(entry["references"] for entry in observations), [1, 1, 7])
        without_alias = self.build(two, one)
        self.assertEqual(without_alias["symbols"], result["symbols"])
        self.assertEqual(without_alias["observations"], result["observations"])

    def test_equal_summary_provider_drift_requires_explicit_audit_selection(self):
        first = self.write("first.json", audit([row()]))
        second = self.write("second.json", audit([row(providers=[LIBRARY, "libExample.native.prx"])]))
        one = self.manifest(self.title(audit_path=first), name="one.json")
        two = self.manifest(self.title(audit_path=second), name="two.json")
        output = self.directory / "old.json"
        output.write_text("previous output", encoding="utf-8")
        self.cli("build", one, two, "--json", output, expected=2)
        self.assertEqual(output.read_text(encoding="utf-8"), "previous output")
        self.assertEqual(self.symbol(self.build(two))["provider_candidates"], ["libExample.native.prx", LIBRARY])

    def test_requested_libraries_providers_and_all_five_classes_remain_distinct(self):
        records = [row(library="libFirst.prx", providers=["libFirst.prx"]),
                   row(library="libSecond.prx", providers=["libSecond.prx"]),
                   row(BETA, BETA_NAME, kind="stub", providers=[LIBRARY, "libExample.native.prx"]),
                   row("absent-symbol", None, library="", kind="absent", providers=[]),
                   row("implemented-symbol", None, library=None, kind="implemented", providers=[LIBRARY, "libExample.native.prx"]),
                   row("module-symbol", None, library="engine.prx", kind="module", providers=["engine.prx"])]
        path = self.write("audit.json", audit(records))
        result = self.build(self.manifest(self.title(audit_path=path)))
        first = self.symbol(result)
        self.assertEqual(first["gap_title_count"], 1)
        libraries = [entry for entry in result["libraries"] if entry["symbol"] == ALPHA]
        self.assertEqual({entry["library"] for entry in libraries}, {"libFirst.prx", "libSecond.prx"})
        self.assertTrue(all(entry["imported_title_count"] == 1 for entry in libraries))
        self.assertEqual(self.symbol(result, BETA)["provider_candidates"], ["libExample.native.prx", LIBRARY])
        self.assertFalse(any(entry["symbol"] == BETA for entry in result["providers"]))
        self.assertEqual(self.symbol(result, "implemented-symbol")["gap_title_count"], 0)
        self.assertEqual(self.symbol(result, "module-symbol")["gap_title_count"], 0)
        self.assertEqual({entry["class"] for entry in result["observations"]}, set(CLASSES))
        raw_libraries = {entry["library"] for entry in result["observations"]}
        self.assertIn(None, raw_libraries)
        self.assertIn("", raw_libraries)

    def test_weak_imports_and_loader_issues_do_not_become_runtime_stops(self):
        records = [row("weak-symbol", None, kind="absent", providers=[], weak=True),
                   row("strong-symbol", None, kind="absent", providers=[]),
                   row("resolved-symbol", None, kind="implemented", library_mismatch=True,
                       symbol_mismatch=True, unsearched_providers=["libOther.prx"])]
        path = self.write("audit.json", audit(records, missing_libraries={"libDeclared.prx": 0},
                          needed_libraries=["libDeclared.prx"],
                          missing_host_dependencies=[{"importer": LIBRARY, "chain": ["host-one", "host-two"]}],
                          stops=[{"registry": "deleted.registry.json", "nid": "strong-symbol", "library": LIBRARY, "name": None}]))
        result = self.build(self.manifest(self.title(audit_path=path)))
        kinds = {entry["kind"] for entry in result["issues"]}
        self.assertTrue({"missing_library", "missing_host_dependency", "library_mismatch", "symbol_mismatch",
                         "unsearched_provider", "static_unresolved_import"}.issubset(kinds))
        static = [entry for entry in result["issues"] if entry["kind"] == "static_unresolved_import"]
        self.assertFalse(any(entry.get("symbol") == "weak-symbol" for entry in static))
        self.assertFalse(any(entry["kind"] in {"native_attributed_stop", "manually_reported_stop"} for entry in result["observations"]))
        self.assertFalse(any(entry["symbol"] == "libDeclared.prx" for entry in result["symbols"]))

    def test_legacy_counting_policy_retains_source_rows_without_repair(self):
        rows = [row(), row(library=None, module="engine.prx")]
        path = self.write("audit.json", audit(rows))
        result = self.build(self.manifest(self.title(audit_path=path)))
        self.assertEqual(len(result["observations"]), 2)
        self.assertEqual([entry["class"] for entry in result["observations"]], ["partial", "partial"])
        self.assertEqual(self.symbol(result)["imported_title_count"], 1)
        self.assertIsNone(result["titles"][0]["audit"]["counting_policy"])
        self.assertEqual(result["titles"][0]["audit"]["source_commit"], "a" * 40)
        self.assertTrue(result["titles"][0]["audit"]["source_dirty"])

    def test_caught_calls_and_unique_low_count_fatal_target_are_separate(self):
        functions = [{"library": "libExample", "name": ALPHA_NAME, "calls": 9000},
                     {"library": "libExample", "name": BETA_NAME, "calls": 1}]
        first_audit = self.write("first-audit.json", audit([row(), row(BETA, BETA_NAME)]))
        second_audit = self.write("second-audit.json", audit([row(references=2), row(BETA, BETA_NAME)]))
        caught = self.write("caught.json", runtime(functions=functions))
        fatal = runtime("PPSA00002", functions=functions, fatal=True)
        fatal["stop"]["message"] = f"Called libExample {BETA_NAME}, which is not implemented."
        stopped = self.write("fatal.json", fatal)
        result = self.build(self.manifest(self.title(audit_path=first_audit, runtime_paths=[caught]),
                                          self.title("PPSA00002", second_audit, [stopped])))
        self.assertEqual(self.symbol(result)["observed_call_title_count"], 2)
        self.assertEqual(self.symbol(result)["native_stop_title_count"], 0)
        self.assertEqual(self.symbol(result, BETA)["native_stop_titles"], ["ps5:PPSA00002"])
        native = [entry for entry in result["observations"] if entry["kind"] == "native_attributed_stop"]
        self.assertEqual(len(native), 1)
        self.assertEqual(native[0]["symbol"], BETA)
        self.assertEqual(native[0]["evidence"]["pointer"], "/stop")
        self.assertEqual(native[0]["evidence"]["sha256"], self.digest(stopped))
        for collection, key in (("libraries", "library"), ("providers", "provider")):
            projected = next(entry for entry in result[collection] if entry["symbol"] == BETA and entry[key] == LIBRARY)
            self.assertEqual(projected["observed_call_title_count"], 2)
            self.assertEqual(projected["native_stop_title_count"], 1)

    def test_ambiguous_internal_and_omitted_fatal_subjects_stay_unassigned(self):
        path = self.write("audit.json", audit([row(providers=[LIBRARY, "libExample.native.prx"]), row(BETA, BETA_NAME)]))
        reports = []
        internal = runtime(run_id="internal", functions=[{"library": None, "name": "NGS2_voice_param_3", "calls": 1}], fatal=True)
        internal["stop"]["message"] = "Called NGS2_voice_param_3, which is not implemented."
        reports.append(internal)
        ambiguous = runtime(run_id="ambiguous", fatal=True, functions=[
            {"library": "libExample", "name": ALPHA_NAME, "calls": 1},
            {"library": "libExample", "name": BETA_NAME, "calls": 5}])
        reports.append(ambiguous)
        unmatched = runtime(run_id="unmatched", fatal=True)
        unmatched["stop"]["message"] = "A producer message without a supported canonical identity"
        reports.append(unmatched)
        omitted = runtime(run_id="omitted", fatal=True)
        omitted["truncated"]["functions"] = 2
        omitted["stop"]["message"] = f"Called libExample {BETA_NAME}, which is not implemented."
        reports.append(omitted)
        paths = [self.write(f"run-{number}.json", document) for number, document in enumerate(reports)]
        result = self.build(self.manifest(self.title(audit_path=path, runtime_paths=paths)))
        stops = [entry for entry in result["observations"] if entry["kind"] in {"native_attributed_stop", "unassigned_runtime_stop"}]
        self.assertEqual(len(stops), 4)
        self.assertTrue(all(entry["symbol"] is None for entry in stops))
        self.assertEqual(self.symbol(result)["native_stop_title_count"], 0)
        self.assertEqual(self.symbol(result, BETA)["native_stop_title_count"], 0)

    def test_manual_log_claims_retain_attribution_and_nonexport_operations(self):
        log = self.directory / "stderr.log"
        log.write_text("first\n  what(): example failure\nNGS2: voice param 3\n", encoding="utf-8")
        claims = [{"log": {"path": log.name, "expected_sha256": self.digest(log)}, "line_start": 2, "line_end": 2,
                   "excerpt": "  what(): example failure", "subject": {"kind": "function", "symbol": ALPHA, "library": LIBRARY}},
                  {"log": {"path": log.name}, "line_start": 3, "line_end": 3, "excerpt": "NGS2: voice param 3",
                   "subject": {"kind": "operation", "name": "NGS2 voice parameter"}}]
        path = self.write("audit.json", audit([row()]))
        result = self.build(self.manifest(self.title(audit_path=path, stop_claims=claims)))
        self.assertEqual(self.symbol(result)["manual_stop_title_count"], 1)
        self.assertIsNone(self.symbol(result)["native_stop_title_count"])
        logs = [entry for entry in result["observations"] if "line_start" in entry["evidence"]]
        self.assertEqual(len(logs), 2)
        self.assertEqual({entry["evidence"]["line_start"] for entry in logs}, {2, 3})
        self.assertTrue(all(entry["evidence"]["sha256"] == self.digest(log) for entry in logs))
        operation = next(entry for entry in logs if entry["evidence"]["line_start"] == 3)
        self.assertIsNone(operation["symbol"])
        self.assertTrue(operation["next_actions"])
        self.assertFalse(any(entry["kind"] == "native_attributed_stop" for entry in logs))

    def test_log_claim_requires_exact_excerpt_range_and_expected_hash(self):
        log = self.directory / "stderr.log"
        log.write_bytes("intro\r\nMálaga failure\r\nlast\r\n".encode("utf-8"))
        base = {"log": {"path": log.name, "expected_sha256": self.digest(log)}, "line_start": 2,
                "line_end": 2, "excerpt": "Málaga failure", "subject": {"kind": "unassigned"}}
        valid = self.manifest(self.title(stop_claims=[base]))
        self.assertEqual(self.build(valid)["coverage"]["log_claim_titles"], 1)
        variants = [dict(base, excerpt="failure"), dict(base, line_start=99, line_end=99),
                    dict(base, line_start=2, line_end=1), dict(base, line_start=True),
                    dict(base, log={"path": log.name, "expected_sha256": "0" * 64})]
        for number, claim in enumerate(variants):
            with self.subTest(case=number):
                manifest = self.manifest(self.title(stop_claims=[claim]))
                self.cli("build", manifest, "--format", "json", expected=2)

    def test_documented_name_only_claim_stays_a_manual_name_fact(self):
        log = self.directory / "stderr.log"
        log.write_text(f"  what():  {ALPHA_NAME} not implemented\n", encoding="utf-8")
        claim = {"log": {"path": log.name}, "line_start": 1, "line_end": 1,
                 "excerpt": f"  what():  {ALPHA_NAME} not implemented",
                 "subject": {"kind": "function-name", "name": ALPHA_NAME}}
        path = self.write("audit.json", audit([row(providers=[LIBRARY, "libExample.native.prx"])]))
        result = self.build(self.manifest(self.title(audit_path=path, stop_claims=[claim])))
        observation = next(entry for entry in result["observations"] if "line_start" in entry["evidence"])
        self.assertEqual(observation["name"], ALPHA_NAME)
        self.assertIsNone(observation["symbol"])
        self.assertIsNone(observation["library"])
        self.assertFalse(observation["providers"])
        self.assertNotEqual(observation["kind"], "native_attributed_stop")
        self.assertEqual(self.symbol(result)["manual_stop_title_count"], 0)

    def test_runtime_only_evidence_and_unavailable_or_lower_bound_coverage(self):
        documents = []
        for index in range(5):
            report = runtime(f"PPSA0000{index + 1}", f"run-{index}")
            if index == 0:
                report["coverage"]["missing"] = False
                report["missing"]["functions"] = []
            elif index == 1:
                report["truncated"]["functions"] = 3
            elif index == 2:
                report["run"].update(recovered=True, incomplete=True, exit="killed", exitCode=None)
                report["stop"].update(kind="other", reason="unclean-unknown", message="Recovered from a checkpoint.")
            elif index == 3:
                report["run"]["incomplete"] = True
            else:
                report["missing"]["functions"] = []
            documents.append(self.write(f"run-{index}.json", report))
        titles = [self.title(f"PPSA0000{index + 1}", runtime_paths=[path]) for index, path in enumerate(documents)]
        result = self.build(self.manifest(*titles))
        self.assertEqual(result["coverage"]["audited_titles"], 0)
        self.assertEqual(result["coverage"]["runtime_titles"], 5)
        self.assertTrue(all(entry["audit"] is None for entry in result["titles"]))
        states = {entry["title_id"]: entry["runtime_coverage"] for entry in result["titles"]}
        self.assertEqual(states["PPSA00001"], "unavailable")
        for title_id in ("PPSA00002", "PPSA00003", "PPSA00004"):
            self.assertEqual(states[title_id], "lower-bound")
        self.assertEqual(states["PPSA00005"], "complete")
        self.assertFalse(any(entry["kind"] == "import_presence" for entry in result["observations"]))

    def test_runtime_title_conflict_and_changed_same_run_snapshot_require_selection(self):
        original = self.write("original.json", runtime())
        conflict = self.manifest(self.title("PPSA00002", runtime_paths=[original]))
        self.cli("build", conflict, "--format", "json", expected=2)
        changed = runtime()
        changed["missing"]["functions"][0]["calls"] = 55
        second = self.write("snapshot.json", changed)
        manifest = self.manifest(self.title(runtime_paths=[original, second]))
        self.cli("build", manifest, "--format", "json", expected=2)
        manifest = self.manifest(self.title(runtime_paths=[original, original]))
        result = self.build(manifest)
        self.assertEqual(result["coverage"]["runtime_titles"], 1)
        self.assertEqual(len([entry for entry in result["observations"] if entry["kind"] == "observed_unimplemented_call"]), 1)

    def test_runtime_build_and_content_version_are_not_borrowed_from_audit(self):
        path = self.write("audit.json", audit([row()]))
        report = runtime()
        report["game"]["contentVersion"] = "09.000.000"
        run = self.write("runtime.json", report)
        conflict = self.manifest(self.title(audit_path=path, runtime_paths=[run], content_version="01.000.000"))
        self.cli("build", conflict, "--format", "json", expected=2)
        result = self.build(self.manifest(self.title(audit_path=path, runtime_paths=[run])))
        title = result["titles"][0]
        self.assertIsNone(title["content_version"])
        self.assertEqual(title["audit"]["source_commit"], "a" * 40)
        serialized = json.dumps(title["runs"])
        self.assertIn("b" * 40, serialized)
        self.assertIn("09.000.000", serialized)
        self.assertIsNone(title["audit"]["provider_build"])

    def test_candidate_coverage_cannot_borrow_another_titles_runtime_or_log(self):
        first = self.write("first.json", audit([row()]))
        second = self.write("second.json", audit([row(BETA, BETA_NAME)]))
        report = runtime("PPSA00002", functions=[{"library": "libExample", "name": BETA_NAME, "calls": 1}])
        report["run"]["incomplete"] = True
        native = self.write("native.json", report)
        log = self.directory / "stderr.log"
        log.write_text("reported Beta stop\n", encoding="utf-8")
        claim = {"log": {"path": log.name}, "line_start": 1, "line_end": 1, "excerpt": "reported Beta stop",
                 "subject": {"kind": "function", "symbol": BETA, "library": LIBRARY}}
        manifest = self.manifest(self.title(audit_path=first),
                                 self.title("PPSA00002", second, [native], stop_claims=[claim]))
        result = self.build(manifest)
        alpha = self.symbol(result)
        self.assertIsNone(alpha["observed_call_title_count"])
        self.assertIsNone(alpha["native_stop_title_count"])
        self.assertIsNone(alpha["manual_stop_title_count"])
        self.assertEqual(alpha["runtime_coverage"], "unavailable")
        beta = self.symbol(result, BETA)
        self.assertEqual(beta["observed_call_title_count"], 1)
        self.assertEqual(beta["manual_stop_title_count"], 1)
        self.assertEqual(beta["runtime_coverage"], "lower-bound")

    def test_malformed_audits_fail_without_replacing_output(self):
        valid = audit([row()])
        mutations = [("references", True), ("references", -1), ("references", 99),
                     ("unique_imports", 2), ("source_dirty", "false")]
        variants = []
        for key, value in mutations:
            document = copy.deepcopy(valid)
            document[key] = value
            variants.append(document)
        for field, value in (("references", False), ("references", -3), ("class", "complete"), ("providers", "libExample.prx")):
            document = copy.deepcopy(valid)
            document["imports"][0][field] = value
            variants.append(document)
        document = copy.deepcopy(valid)
        document["unique_by_class"]["partial"] = 0
        variants.append(document)
        variants.extend([dict(valid, missing_host_dependencies={}), dict(valid, missing_host_dependencies="")])
        output = self.directory / "result.json"
        for number, document in enumerate(variants):
            with self.subTest(case=number):
                path = self.write("audit.json", document)
                manifest = self.manifest(self.title(audit_path=path))
                output.write_text("original output", encoding="utf-8")
                self.cli("build", manifest, "--json", output, expected=2)
                self.assertEqual(output.read_text(encoding="utf-8"), "original output")

    def test_duplicate_keys_nonfinite_versions_hashes_and_missing_files_fail(self):
        valid = self.write("audit.json", audit([row()]))
        title = self.title(audit_path=valid)
        manifest = self.manifest(title)
        invalid_audits = [valid.read_text(encoding="utf-8").replace('"references": 1', '"references": NaN', 1),
                          valid.read_text(encoding="utf-8").replace('"source_dirty": true', '"source_dirty": true, "source_dirty": false', 1)]
        for number, text in enumerate(invalid_audits):
            with self.subTest(audit=number):
                valid.write_text(text, encoding="utf-8")
                self.cli("build", manifest, "--format", "json", expected=2)
        self.write("audit.json", audit([row()]))
        for field, value in (("version", 2), ("version", True), ("format", "other")):
            document = {"format": "anyps5-import-corpus", "version": 1, "titles": [title], field: value}
            bad = self.write("corpus.json", document)
            self.cli("build", bad, "--format", "json", expected=2)
        for reference in ({"path": "missing.json"}, {"path": valid.name, "expected_sha256": "0" * 64}):
            bad = self.manifest(self.title(audit=reference))
            self.cli("build", bad, "--format", "json", expected=2)
        report = runtime()
        report["version"] = 2
        native = self.write("native.json", report)
        self.cli("build", self.manifest(self.title(runtime_paths=[native])), "--format", "json", expected=2)

    def test_native_schema_rejects_disabled_positive_counts_and_false_recovery(self):
        variants = []
        for field, value in (("calls", True), ("calls", -1), ("calls", float("inf"))):
            document = runtime()
            document["missing"]["functions"][0][field] = value
            variants.append(document)
        disabled = runtime()
        disabled["coverage"]["missing"] = False
        variants.append(disabled)
        recovered = runtime()
        recovered["run"]["recovered"] = True
        variants.append(recovered)
        enormous = runtime()
        enormous["run"]["framesPresented"] = 10 ** 400
        variants.append(enormous)
        for number, document in enumerate(variants):
            with self.subTest(case=number):
                path = self.write("runtime.json", document)
                manifest = self.manifest(self.title(runtime_paths=[path]))
                self.cli("build", manifest, "--format", "json", expected=2)

    def test_guest_platform_and_title_ids_are_not_merged_by_shared_names(self):
        first = self.write("ps5-audit.json", audit([row()]))
        second = self.write("ps4-audit.json", audit([row(references=2)]))
        result = self.build(self.manifest(self.title(audit_path=first, target_platform="linux"),
                                          self.title("CUSA00001", second, guest_platform="ps4", target_platform="windows")))
        self.assertEqual(self.symbol(result)["imported_titles"], ["ps4:CUSA00001", "ps5:PPSA00001"])
        self.assertEqual(self.symbol(result)["imported_title_count"], 2)
        self.assertEqual({entry["guest_platform"] for entry in result["titles"]}, {"ps4", "ps5"})

    def test_unnamed_module_symbol_is_preserved_without_export_aggregation(self):
        record = row("", None, library=None, kind="absent", providers=[], module="engine.prx")
        path = self.write("audit.json", audit([record]))
        result = self.build(self.manifest(self.title(audit_path=path)))
        observation = next(entry for entry in result["observations"] if entry["kind"] == "import_presence")
        self.assertIsNone(observation["symbol"])
        self.assertEqual(observation["raw_symbol"], "")
        self.assertEqual(observation["class"], "absent")
        self.assertEqual(observation["references"], 1)
        self.assertEqual(observation["evidence"]["pointer"], "/imports/0")
        self.assertTrue(any(entry["kind"] == "unnamed_module_import" for entry in result["issues"]))
        self.assertFalse(any(entry["symbol"] == "" for entry in result["symbols"]))
        invalid = self.write("audit.json", audit([row("", None, kind="absent", providers=[])]))
        self.cli("build", self.manifest(self.title(audit_path=invalid)), "--format", "json", expected=2)

    def test_partial_mode_accounts_for_failed_evidence_and_cannot_succeed_empty(self):
        path = self.write("audit.json", audit([row()]))
        manifest = self.manifest(self.title(audit_path=path), self.title("PPSA00002", audit={"path": "missing.json"}))
        output = self.directory / "partial.json"
        result = self.cli("build", manifest, "--json", output, "--allow-incomplete", expected=2)
        document = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(document["status"], "incomplete")
        self.assertEqual(document["coverage"]["selected_titles"], 2)
        self.assertEqual(document["coverage"]["accepted_titles"], 1)
        self.assertEqual(document["coverage"]["failed_evidence"], 1)
        self.assertTrue(document["errors"])
        self.assertIn("incomplete", (result.stdout + result.stderr).lower())
        empty = self.manifest(self.title(audit={"path": "missing.json"}))
        previous = output.read_bytes()
        self.cli("build", empty, "--allow-incomplete", "--json", output, expected=2)
        self.assertEqual(output.read_bytes(), previous)

    def test_canonical_result_is_independent_of_order_and_corpus_location(self):
        first = self.write("first.json", audit([row()]))
        second = self.write("second.json", audit([row(BETA, BETA_NAME)]))
        one = self.manifest(self.title(audit_path=first), name="one.json")
        two = self.manifest(self.title("PPSA00002", second), name="two.json")
        expected = self.cli("build", one, two, "--format", "json").stdout
        reversed_result = self.cli("build", two, one, "--format", "json", environment={"LC_ALL": "C", "PYTHONIOENCODING": "ascii"})
        self.assertEqual(expected, reversed_result.stdout)
        moved = self.directory / "relocated"
        moved.mkdir()
        for path in (first, second, one, two):
            shutil.copyfile(path, moved / path.name)
        actual = self.cli("build", moved / one.name, moved / two.name, "--format", "json").stdout
        self.assertEqual(expected, actual)

    def test_output_cannot_alias_audit_or_manifest(self):
        path = self.write("audit.json", audit([row()]))
        manifest = self.manifest(self.title(audit_path=path))
        before = {entry: entry.read_bytes() for entry in (path, manifest)}
        for source in (path, manifest):
            with self.subTest(path=source.name):
                self.cli("build", manifest, "--json", source, expected=2)
                self.assertEqual(source.read_bytes(), before[source])
        hardlink = self.directory / "hardlink.json"
        os.link(path, hardlink)
        self.cli("build", manifest, "--json", hardlink, expected=2)
        self.assertEqual(path.read_bytes(), before[path])
        symbolic = self.directory / "symbolic.json"
        try:
            symbolic.symlink_to(path)
        except OSError:
            return
        self.cli("build", manifest, "--json", symbolic, expected=2)
        self.assertEqual(path.read_bytes(), before[path])

    def test_failed_atomic_replace_preserves_previous_output_and_inputs(self):
        import import_census
        path = self.write("audit.json", audit([row()]))
        manifest = self.manifest(self.title(audit_path=path))
        output = self.directory / "result.json"
        output.write_text("previous", encoding="utf-8")
        before = path.read_bytes()
        with patch.object(os, "replace", side_effect=OSError("injected replace failure")), contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            result = import_census.main(["build", str(manifest), "--json", str(output)])
        self.assertEqual(result, 2)
        self.assertEqual(output.read_text(encoding="utf-8"), "previous")
        self.assertEqual(path.read_bytes(), before)

    def test_offline_entry_point_ignores_embedded_paths_and_does_not_execute(self):
        import import_census
        trap = self.directory / "not-an-input.json"
        trap.write_text("invalid metadata that must never be read", encoding="utf-8")
        document = audit([row()], registries=[str(trap), "https://example.invalid/registry.json"])
        path = self.write("audit.json", document)
        manifest = self.manifest(self.title(audit_path=path, identity_source="https://example.invalid/title"))
        consumed = []
        original = Path.read_bytes

        def recording_read(candidate):
            consumed.append(candidate.resolve())
            return original(candidate)

        output = io.StringIO()
        with patch.object(subprocess, "run", side_effect=AssertionError("subprocess execution")), \
                patch.object(urllib.request, "urlopen", side_effect=AssertionError("network fetch")), \
                patch.object(socket, "create_connection", side_effect=AssertionError("network connection")), \
                patch.object(os, "system", side_effect=AssertionError("shell execution")), \
                patch.object(Path, "read_bytes", recording_read), contextlib.redirect_stdout(output), contextlib.redirect_stderr(io.StringIO()):
            result = import_census.main(["build", str(manifest), "--format", "json"])
        self.assertEqual(result, 0)
        self.assertEqual(json.loads(output.getvalue())["status"], "complete")
        self.assertNotIn(trap.resolve(), consumed)

    def test_show_default_read_cap_can_read_the_build_default_output_range(self):
        import import_census
        path = self.write("audit.json", audit([row()]))
        manifest = self.manifest(self.title(audit_path=path))
        saved = self.write("saved.json", self.build(manifest))
        reads = []
        original = Path.open

        class Reader:
            def __init__(self, handle, candidate):
                self.handle = handle
                self.candidate = candidate

            def __enter__(self):
                return self

            def __exit__(self, *arguments):
                self.handle.close()

            def read(self, size):
                reads.append((self.candidate, size))
                return self.handle.read(size)

        def opening(candidate, *arguments, **keywords):
            handle = original(candidate, *arguments, **keywords)
            return Reader(handle, candidate) if candidate in (manifest, saved) else handle

        with patch.object(Path, "open", opening), contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            built = import_census.main(["build", str(manifest), "--format", "json"])
            shown = import_census.main(["show", str(saved), "--symbol", ALPHA, "--format", "json"])
            limited = import_census.main(["show", str(saved), "--symbol", ALPHA, "--max-input-bytes", "128", "--format", "json"])
        self.assertEqual((built, shown, limited), (0, 0, 2))
        self.assertIn((manifest, 64 * 1024 * 1024 + 1), reads)
        self.assertIn((saved, 256 * 1024 * 1024 + 1), reads)
        self.assertIn((saved, 129), reads)

    def test_unc_input_and_output_are_rejected_without_resolving_or_opening(self):
        import import_census
        path = self.write("audit.json", audit([row()]))
        valid = self.manifest(self.title(audit_path=path), name="valid.json")
        network = "//invalid.example/never-open/report.json"
        invalid = self.manifest(self.title(audit={"path": network}), name="invalid.json")
        original_open = Path.open
        original_resolve = Path.resolve

        def local(candidate):
            self.assertFalse(str(candidate).replace("\\", "/").startswith("//"), "attempted network filesystem access")

        def opening(candidate, *arguments, **keywords):
            local(candidate)
            return original_open(candidate, *arguments, **keywords)

        def resolving(candidate, *arguments, **keywords):
            local(candidate)
            return original_resolve(candidate, *arguments, **keywords)

        with patch.object(Path, "open", opening), patch.object(Path, "resolve", resolving), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            rejected_input = import_census.main(["build", str(invalid), "--format", "json"])
            rejected_output = import_census.main(["build", str(valid), "--json", network])
        self.assertEqual((rejected_input, rejected_output), (2, 2))

    def test_human_output_escapes_controls_and_show_retains_exact_handoff(self):
        unsafe = "sceExample\x1b[31m<script>|\nInjected"
        path = self.write("audit.json", audit([row(name=unsafe), row(BETA, BETA_NAME)]))
        manifest = self.manifest(self.title(audit_path=path))
        output = self.directory / "census.json"
        human = self.cli("build", manifest, "--json", output).stdout
        self.assertNotIn("\x1b", human)
        self.assertNotIn("<script>", human)
        shown = json.loads(self.cli("show", output, "--symbol", ALPHA, "--format", "json").stdout)
        self.assertEqual(shown["selection"]["symbol"], ALPHA)
        self.assertEqual({entry["symbol"] for entry in shown["symbols"]}, {ALPHA})
        self.assertTrue(shown["observations"])
        self.assertTrue(all(entry["symbol"] == ALPHA for entry in shown["observations"]))
        self.assertTrue(all(entry["evidence"]["sha256"] == self.digest(path) for entry in shown["observations"]))
        self.assertTrue(all(entry["next_actions"] for entry in shown["observations"]))
        by_name = json.loads(self.cli("show", output, "--symbol", BETA_NAME, "--format", "json").stdout)
        self.assertEqual({entry["symbol"] for entry in by_name["symbols"]}, {BETA})
        missing = json.loads(self.cli("show", output, "--symbol", "never-imported", "--format", "json").stdout)
        self.assertEqual(missing["observations"], [])

    def test_show_title_filter_recomputes_evidence_and_class_title_sets(self):
        first = self.write("first.json", audit([row()]))
        second = self.write("second.json", audit([row(kind="stub")]))
        manifest = self.manifest(self.title(audit_path=first), self.title("PPSA00002", second))
        output = self.directory / "census.json"
        self.cli("build", manifest, "--json", output)
        selected = json.loads(self.cli("show", output, "--symbol", ALPHA, "--title", "PPSA00001", "--format", "json").stdout)
        summary = self.symbol(selected)
        self.assertEqual(summary["imported_titles"], ["ps5:PPSA00001"])
        self.assertEqual(summary["imported_title_count"], 1)
        self.assertEqual(summary["by_class"]["partial"], ["ps5:PPSA00001"])
        self.assertEqual(summary["by_class"]["stub"], [])
        self.assertTrue(all(entry["title_key"] == "ps5:PPSA00001" for entry in selected["observations"]))

    def test_show_name_query_keeps_unnamed_observations_for_the_same_symbol(self):
        first = self.write("first.json", audit([row()]))
        second = self.write("second.json", audit([row(name=None, kind="implemented")]))
        manifest = self.manifest(self.title(audit_path=first), self.title("PPSA00002", second))
        output = self.directory / "census.json"
        self.cli("build", manifest, "--json", output)
        selected = json.loads(self.cli("show", output, "--symbol", ALPHA_NAME, "--format", "json").stdout)
        self.assertEqual({entry["title_key"] for entry in selected["observations"]}, {"ps5:PPSA00001", "ps5:PPSA00002"})
        self.assertEqual(self.symbol(selected)["imported_title_count"], 2)
        self.assertEqual(self.symbol(selected)["by_class"]["implemented"], ["ps5:PPSA00002"])

    def test_show_rejects_unsupported_and_corrupt_saved_census(self):
        path = self.write("audit.json", audit([row()]))
        valid = self.build(self.manifest(self.title(audit_path=path)))
        variants = [dict(valid, format="other"), dict(valid, version=2), dict(valid, version=True),
                    dict(valid, observations="not a list")]
        corrupt = copy.deepcopy(valid)
        corrupt["symbols"][0]["imported_title_count"] = True
        variants.append(corrupt)
        for number, document in enumerate(variants):
            with self.subTest(case=number):
                saved = self.write("saved.json", document)
                self.cli("show", saved, "--symbol", ALPHA, "--format", "json", expected=2)


if __name__ == "__main__":
    unittest.main()
