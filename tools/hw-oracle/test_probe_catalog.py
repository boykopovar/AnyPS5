import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


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


if __name__ == "__main__":
    unittest.main()
