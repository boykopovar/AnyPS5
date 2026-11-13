import copy
import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

try:
    import probe_catalog
except ModuleNotFoundError as error:
    if error.name != "probe_catalog":
        raise
    probe_catalog = None


def require_probe_catalog(test):
    test.assertIsNotNone(probe_catalog, "probe_catalog.py is required by these regression tests")


class CatalogLoadingTests(unittest.TestCase):
    def setUp(self):
        require_probe_catalog(self)
        self.catalog, self.digest = probe_catalog.read_catalog()

    def test_catalog_has_unique_curated_probes_and_a_content_digest(self):
        identifiers = [probe["id"] for probe in self.catalog["probes"]]
        self.assertEqual(len(identifiers), len(set(identifiers)))
        self.assertEqual(len(self.digest), 64)
        self.assertTrue(all(len(probe["rows"]) > 0 for probe in self.catalog["probes"]))

    def test_hexadecimal_and_decimal_words_are_normalized(self):
        probe = self.catalog["probes"][0]
        self.assertEqual(probe["rows"][0]["input"], [0, 0, 0, 0])
        self.assertEqual(probe["rows"][3]["input"], [0x7f7fffff, 0x7f7fffff, 0, 0])

    def test_catalog_reader_reports_invalid_json_and_missing_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "catalog"
            path.write_text("{", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "cannot read probe catalog"):
                probe_catalog.read_catalog(path)
            with self.assertRaisesRegex(ValueError, "cannot read probe catalog"):
                probe_catalog.read_catalog(path.with_name("missing"))

    def test_catalog_rejects_unsupported_versions_and_duplicate_probe_ids(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "catalog"
            unsupported = copy.deepcopy(self.catalog)
            unsupported["schema_version"] = 2
            path.write_text(json.dumps(unsupported), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "unsupported schema_version"):
                probe_catalog.read_catalog(path)
            duplicate = copy.deepcopy(self.catalog)
            duplicate["probes"].append(copy.deepcopy(duplicate["probes"][0]))
            path.write_text(json.dumps(duplicate), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate probe id"):
                probe_catalog.read_catalog(path)

    def test_probe_ids_and_catalog_shape_are_restricted(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "catalog"
            for invalid in ([], {"schema_version": 1, "probes": []},
                            {"schema_version": 1, "probes": self.catalog["probes"], "extra": True}):
                with self.subTest(invalid=type(invalid).__name__):
                    path.write_text(json.dumps(invalid), encoding="utf-8")
                    with self.assertRaises(ValueError):
                        probe_catalog.read_catalog(path)
            malformed = copy.deepcopy(self.catalog)
            malformed["probes"][0]["id"] = "not/a/path"
            path.write_text(json.dumps(malformed), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "id must contain lowercase"):
                probe_catalog.read_catalog(path)

    def test_instruction_body_must_start_with_the_declared_form(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "catalog"
            malformed = copy.deepcopy(self.catalog)
            malformed["probes"][0]["body"] = "v_sub_f32 v10, v4, v5"
            path.write_text(json.dumps(malformed), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "one uncommented declared instruction"):
                probe_catalog.read_catalog(path)
            malformed["probes"][0]["instruction"] = "bad mnemonic"
            path.write_text(json.dumps(malformed), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "instruction must be an assembly mnemonic"):
                probe_catalog.read_catalog(path)

    def test_probe_body_cannot_add_implicit_instructions_or_expected_results(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "catalog"
            malformed = copy.deepcopy(self.catalog)
            malformed["probes"][0]["body"] += "\n  s_endpgm"
            path.write_text(json.dumps(malformed), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "one uncommented declared instruction"):
                probe_catalog.read_catalog(path)
            malformed = copy.deepcopy(self.catalog)
            malformed["probes"][0]["expected"] = [0]
            path.write_text(json.dumps(malformed), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "unknown expected"):
                probe_catalog.read_catalog(path)

    def test_probe_rows_reject_duplicate_names_and_non_four_word_inputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "catalog"
            malformed = copy.deepcopy(self.catalog)
            malformed["probes"][0]["rows"][1]["name"] = malformed["probes"][0]["rows"][0]["name"]
            path.write_text(json.dumps(malformed), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate row name"):
                probe_catalog.read_catalog(path)
            malformed = copy.deepcopy(self.catalog)
            malformed["probes"][0]["rows"][0]["input"] = [0, 1, 2]
            path.write_text(json.dumps(malformed), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "four dwords"):
                probe_catalog.read_catalog(path)

    def test_probe_rows_reject_boolean_negative_and_overflow_words(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "catalog"
            for value, message in ((True, "unsigned 32-bit integer or numeric string"),
                                   (-1, "unsigned 32-bit integer"),
                                   (0x100000000, "unsigned 32-bit integer"),
                                   (1.5, "unsigned 32-bit integer or numeric string")):
                with self.subTest(value=value):
                    malformed = copy.deepcopy(self.catalog)
                    malformed["probes"][0]["rows"][0]["input"][0] = value
                    path.write_text(json.dumps(malformed), encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, message):
                        probe_catalog.read_catalog(path)

    def test_probe_word_strings_accept_only_decimal_or_hexadecimal(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "catalog"
            for value in ("0b101", "0o17", "+1", "-1", "08", ""):
                with self.subTest(value=value):
                    malformed = copy.deepcopy(self.catalog)
                    malformed["probes"][0]["rows"][0]["input"][0] = value
                    path.write_text(json.dumps(malformed), encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, "numeric string"):
                        probe_catalog.read_catalog(path)

    def test_probe_modes_and_dimensions_are_checked_before_execution(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "catalog"
            cases = (
                ("wave_size", 16, "wave_size must be 32 or 64"),
                ("output_registers", 0, "output_registers must be from 1 to 16"),
                ("lds_bytes", 65537, "lds_bytes must be from 0"),
                ("memory", "shared", "memory must be fine or coarse"),
            )
            for field, value, message in cases:
                with self.subTest(field=field):
                    malformed = copy.deepcopy(self.catalog)
                    malformed["probes"][0][field] = value
                    path.write_text(json.dumps(malformed), encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, message):
                        probe_catalog.read_catalog(path)
            malformed = copy.deepcopy(self.catalog)
            malformed["probes"][0]["floating_point_modes"]["ieee"] = True
            path.write_text(json.dumps(malformed), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "mode ieee must be"):
                probe_catalog.read_catalog(path)


class ProbeGenerationTests(unittest.TestCase):
    def setUp(self):
        require_probe_catalog(self)
        self.catalog, self.digest = probe_catalog.read_catalog()

    def test_generation_writes_assembly_rows_and_catalog_metadata(self):
        probe = self.catalog["probes"][0]
        with tempfile.TemporaryDirectory() as temporary:
            generated = probe_catalog.generate([probe], self.digest, Path(temporary) / "generated")
            case = generated[0]
            self.assertEqual(case.name, probe["id"])
            self.assertEqual((case / "body.s").read_text(encoding="utf-8"), probe["body"] + "\n")
            rows = (case / "rows.txt").read_text(encoding="utf-8").splitlines()
            self.assertEqual(len(rows), len(probe["rows"]))
            self.assertEqual(rows[0], "0x00000000 0x00000000 0x00000000 0x00000000")
            metadata = json.loads((case / "probe.json").read_text(encoding="utf-8"))
            self.assertEqual(metadata["catalog_sha256"], self.digest)
            self.assertEqual(metadata["probe_sha256"], probe_catalog.probe_digest(probe))
            self.assertEqual(metadata["rows"], probe["rows"])

    def test_every_curated_input_bundle_passes_the_existing_oracle_validator(self):
        with tempfile.TemporaryDirectory() as temporary:
            for probe in self.catalog["probes"]:
                with self.subTest(probe=probe["id"]):
                    case = probe_catalog.generate([probe], self.digest, Path(temporary) / probe["id"])[0]
                    argv = ["hw_oracle.py", str(case / "body.s"), str(case / "rows.txt"),
                            "--validate-only", "--json", "--outs", str(probe["output_registers"])]
                    argv.extend(argument for name, value in probe["floating_point_modes"].items()
                                for argument in ("--" + name.replace("_", "-"), str(value)))
                    if probe["wave_size"] == 64:
                        argv.append("--wave64")
                    if probe["memory"] == "coarse":
                        argv.append("--coarse")
                    argv.extend(("--lds", str(probe["lds_bytes"])))
                    output = io.StringIO()
                    with patch.object(sys, "argv", argv), contextlib.redirect_stdout(output):
                        self.assertEqual(probe_catalog.hw_oracle.main(), 0)
                    report = json.loads(output.getvalue())
                    self.assertEqual(report["rows"], len(probe["rows"]))
                    self.assertEqual(report["wave_size"], probe["wave_size"])

    def test_selection_rejects_unknown_and_repeated_probe_ids(self):
        with self.assertRaisesRegex(ValueError, "unknown probe id"):
            probe_catalog.select_probes(self.catalog["probes"], ["missing-probe"])
        identifier = self.catalog["probes"][0]["id"]
        with self.assertRaisesRegex(ValueError, "selected more than once"):
            probe_catalog.select_probes(self.catalog["probes"], [identifier, identifier])

    def test_generation_does_not_replace_existing_case_directories(self):
        probe = self.catalog["probes"][0]
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "generated"
            case = output / probe["id"]
            case.mkdir(parents=True)
            marker = case / "keep.txt"
            marker.write_text("existing", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "refusing to replace generated probe directories"):
                probe_catalog.generate([probe], self.digest, output)
            self.assertEqual(marker.read_text(encoding="utf-8"), "existing")


class ProbeRunTests(unittest.TestCase):
    def setUp(self):
        require_probe_catalog(self)
        self.catalog, self.digest = probe_catalog.read_catalog()

    def test_run_saves_results_with_oracle_settings_and_device_label(self):
        probe = self.catalog["probes"][0]
        outputs = [(index,) * 16 for index in range(len(probe["rows"]))]
        with tempfile.TemporaryDirectory() as temporary:
            with patch.object(probe_catalog.hw_oracle, "runtime", return_value="hsa"), \
                    patch.object(probe_catalog.hw_oracle, "target", return_value="gfx1036"), \
                    patch.object(probe_catalog.hw_oracle, "run", return_value=outputs) as run:
                paths = probe_catalog.run_probes([probe], self.digest, temporary, "rx-6750-xt")
            run.assert_called_once_with(
                probe["body"], [tuple(row["input"]) for row in probe["rows"]],
                wave64=False, coarse=False, **probe["floating_point_modes"], lds=4096,
            )
            report = probe_catalog.compare_results.read_report(paths[0])
            self.assertEqual(report["probe_id"], probe["id"])
            self.assertEqual(report["device_label"], "rx-6750-xt")
            self.assertEqual(report["assembly_target"], "gfx1036")
            self.assertEqual(report["rows"][0]["output"], [0])

    def test_run_rejects_existing_measurements_instead_of_overwriting(self):
        probe = self.catalog["probes"][0]
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "rx-6750-xt" / f"{probe['id']}.json"
            destination.parent.mkdir()
            destination.write_text("keep", encoding="utf-8")
            with patch.object(probe_catalog.hw_oracle, "run") as run:
                with self.assertRaisesRegex(ValueError, "refusing to replace existing measurement"):
                    probe_catalog.run_probes([probe], self.digest, temporary, "rx-6750-xt")
                run.assert_not_called()

    def test_run_rejects_incomplete_gpu_output_without_writing_a_report(self):
        probe = self.catalog["probes"][0]
        with tempfile.TemporaryDirectory() as temporary:
            with patch.object(probe_catalog.hw_oracle, "runtime", return_value="hsa"), \
                    patch.object(probe_catalog.hw_oracle, "target", return_value="gfx1036"), \
                    patch.object(probe_catalog.hw_oracle, "run", return_value=[]):
                with self.assertRaisesRegex(ValueError, "incomplete result"):
                    probe_catalog.run_probes([probe], self.digest, temporary, "rx-6750-xt")
            self.assertEqual(list((Path(temporary) / "rx-6750-xt").glob("*.json")), [])

    def test_run_rejects_non_dword_gpu_results_without_writing_a_report(self):
        probe = self.catalog["probes"][0]
        outputs = [(-1,) * 16 for _ in probe["rows"]]
        with tempfile.TemporaryDirectory() as temporary:
            with patch.object(probe_catalog.hw_oracle, "runtime", return_value="hsa"), \
                    patch.object(probe_catalog.hw_oracle, "target", return_value="gfx1036"), \
                    patch.object(probe_catalog.hw_oracle, "run", return_value=outputs):
                with self.assertRaisesRegex(ValueError, "non-dword result"):
                    probe_catalog.run_probes([probe], self.digest, temporary, "rx-6750-xt")
            self.assertEqual(list((Path(temporary) / "rx-6750-xt").glob("*.json")), [])

    def test_run_rejects_unsafe_device_labels_before_creating_paths(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "device label must contain"):
                probe_catalog.run_probes(self.catalog["probes"][:1], self.digest, temporary, "../gpu")
            self.assertEqual(list(Path(temporary).iterdir()), [])


class ProbeCoverageTests(unittest.TestCase):
    def setUp(self):
        require_probe_catalog(self)
        self.catalog, self.digest = probe_catalog.read_catalog()

    def write_report(self, root, probe, digest=None, device_label="rx-6750-xt"):
        destination = Path(root) / device_label / f"{probe['id']}.json"
        report = probe_catalog.make_result(
            probe, self.digest if digest is None else digest, device_label, "gfx1036", "hsa",
            [(0x3f800000,) * 16 for _ in probe["rows"]],
        )
        probe_catalog.write_atomic(destination, json.dumps(report, indent=2) + "\n")
        return destination

    def test_coverage_distinguishes_missing_probe_reports(self):
        with tempfile.TemporaryDirectory() as temporary:
            results = probe_catalog.coverage(self.catalog["probes"], self.digest, temporary, "rx-6750-xt")
            self.assertEqual([item["status"] for item in results], ["missing"] * len(self.catalog["probes"]))

    def test_coverage_fraction_and_probe_filter_report_the_selected_set(self):
        probes = self.catalog["probes"]
        with tempfile.TemporaryDirectory() as temporary:
            self.write_report(temporary, probes[0])
            self.write_report(temporary, probes[1])
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = probe_catalog.main([
                    "coverage", "--results-dir", temporary, "--device-label", "rx-6750-xt", "--json",
                    "--probe", probes[0]["id"],
                ])
            report = json.loads(output.getvalue())
            self.assertEqual(result, 0)
            self.assertEqual((report["measured"], report["total"], report["percent"]), (1, 1, 100.0))
            all_results = probe_catalog.coverage(probes, self.digest, temporary, "rx-6750-xt")
            self.assertEqual(sum(item["status"] == "measured" for item in all_results), 2)

    def test_coverage_accepts_a_report_from_the_exact_catalog(self):
        probe = self.catalog["probes"][0]
        with tempfile.TemporaryDirectory() as temporary:
            self.write_report(temporary, probe)
            results = probe_catalog.coverage([probe], self.digest, temporary, "rx-6750-xt")
            self.assertEqual(results, [{"id": probe["id"], "status": "measured", "detail": ""}])

    def test_coverage_keeps_unchanged_probes_when_catalog_gains_another_entry(self):
        probe = self.catalog["probes"][0]
        with tempfile.TemporaryDirectory() as temporary:
            self.write_report(temporary, probe, digest="0" * 64)
            results = probe_catalog.coverage([probe], self.digest, temporary, "rx-6750-xt")
            self.assertEqual(results[0]["status"], "measured")

    def test_coverage_does_not_accept_a_report_moved_under_another_device_label(self):
        probe = self.catalog["probes"][0]
        with tempfile.TemporaryDirectory() as temporary:
            other_path = self.write_report(temporary, probe, device_label="other-gpu")
            selected_path = Path(temporary) / "rx-6750-xt" / other_path.name
            selected_path.parent.mkdir()
            selected_path.write_bytes(other_path.read_bytes())
            result = probe_catalog.coverage([probe], self.digest, temporary, "rx-6750-xt")[0]
            self.assertEqual(result["status"], "invalid")
            self.assertIn("device_label differs", result["detail"])

    def test_coverage_marks_changed_probe_definitions_stale(self):
        original = self.catalog["probes"][0]
        changed = copy.deepcopy(original)
        changed["description"] += " Updated definition."
        with tempfile.TemporaryDirectory() as temporary:
            self.write_report(temporary, original)
            results = probe_catalog.coverage([changed], self.digest, temporary, "rx-6750-xt")
            self.assertEqual(results[0]["status"], "stale")
            self.assertIn("probe definition digest", results[0]["detail"])

    def test_coverage_rejects_reports_for_different_inputs_or_settings(self):
        probe = self.catalog["probes"][0]
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_report(temporary, probe)
            report = json.loads(path.read_text(encoding="utf-8"))
            report["rows"][0]["input"][0] ^= 1
            path.write_text(json.dumps(report), encoding="utf-8")
            result = probe_catalog.coverage([probe], self.digest, temporary, "rx-6750-xt")[0]
            self.assertEqual(result["status"], "invalid")
            self.assertIn("input rows differ", result["detail"])
            report["rows"][0]["input"] = probe["rows"][0]["input"]
            report["wave_size"] = 64
            path.write_text(json.dumps(report), encoding="utf-8")
            result = probe_catalog.coverage([probe], self.digest, temporary, "rx-6750-xt")[0]
            self.assertEqual(result["status"], "invalid")
            self.assertIn("wave_size differs", result["detail"])

    def test_coverage_reports_unreadable_json_as_invalid(self):
        probe = self.catalog["probes"][0]
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "rx-6750-xt" / f"{probe['id']}.json"
            path.parent.mkdir()
            path.write_text("{", encoding="utf-8")
            result = probe_catalog.coverage([probe], self.digest, temporary, "rx-6750-xt")[0]
            self.assertEqual(result["status"], "invalid")
            self.assertTrue(result["detail"])


class ProbeCatalogCliTests(unittest.TestCase):
    def setUp(self):
        require_probe_catalog(self)

    def test_list_and_generate_commands_run_without_a_gpu(self):
        listing = io.StringIO()
        with contextlib.redirect_stdout(listing):
            result = probe_catalog.main(["list"])
        self.assertEqual(result, 0)
        self.assertIn("v-add-f32-edge-values", listing.getvalue())
        with tempfile.TemporaryDirectory() as temporary:
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = probe_catalog.main([
                    "generate", "--output-dir", temporary,
                    "--probe", "v-add-f32-edge-values",
                ])
            self.assertEqual(result, 0)
            self.assertTrue((Path(temporary) / "v-add-f32-edge-values" / "body.s").is_file())

    def test_coverage_cli_emits_json_for_missing_measurements(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = probe_catalog.main([
                    "coverage", "--results-dir", temporary, "--device-label", "local-rdna2", "--json",
                ])
            report = json.loads(output.getvalue())
            self.assertEqual(result, 1)
            self.assertEqual(report["measured"], 0)
            self.assertEqual(report["total"], len(probe_catalog.read_catalog()[0]["probes"]))
            self.assertTrue(all(item["status"] == "missing" for item in report["probes"]))

    def test_cli_rejects_unknown_probes_without_running_them(self):
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            with self.assertRaises(SystemExit) as exit:
                probe_catalog.main([
                    "generate", "--output-dir", "/tmp/unused-probe-catalog", "--probe", "unknown",
                ])
        self.assertEqual(exit.exception.code, 2)
        self.assertIn("unknown probe id", stderr.getvalue())

if __name__ == "__main__":
    unittest.main()
