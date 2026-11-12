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


if __name__ == "__main__":
    unittest.main()
