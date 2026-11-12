import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import probe_catalog


class CatalogLoadingTests(unittest.TestCase):
    def setUp(self):
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
            with self.assertRaisesRegex(ValueError, "body must start"):
                probe_catalog.read_catalog(path)
            malformed["probes"][0]["instruction"] = "bad mnemonic"
            path.write_text(json.dumps(malformed), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "instruction must be an assembly mnemonic"):
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
            self.assertEqual(metadata["rows"], probe["rows"])

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

    def test_run_rejects_unsafe_device_labels_before_creating_paths(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "device label must contain"):
                probe_catalog.run_probes(self.catalog["probes"][:1], self.digest, temporary, "../gpu")
            self.assertEqual(list(Path(temporary).iterdir()), [])


if __name__ == "__main__":
    unittest.main()
