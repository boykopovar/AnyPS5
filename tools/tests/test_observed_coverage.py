import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import progress

ROOT = Path(__file__).resolve().parents[2]
PRX = ROOT / "core" / "libs" / "prx"
OBSERVED = ROOT / "docs" / "dev" / "observed"
STUB = "int APS5_VABI {name}() {{ NotImplemented_nid_no_patch(__func__); return 0; }}\n"


class ObservedCoverageTests(unittest.TestCase):
    def test_every_library_directory_has_a_manifest(self):
        libraries = {path.name for path in PRX.iterdir() if path.is_dir()}
        manifests = {path.stem for path in OBSERVED.glob("*.txt")}
        self.assertEqual(sorted(libraries - manifests), [], "add docs/dev/observed/<library>.txt: python3 tools/observed_imports.py classify <library>")
        self.assertEqual(sorted(manifests - libraries), [])

    def test_every_manifest_is_valid(self):
        for path in sorted(OBSERVED.glob("*.txt")):
            with self.subTest(path=path.name):
                text = path.read_text(encoding="utf-8-sig")
                self.assertEqual(progress.manifest_problems(text), [])
                self.assertEqual(progress.manifest_tag_problems(text), [])

    def test_a_library_without_evidence_has_an_empty_classifying_manifest_and_only_unobserved_stubs(self):
        text = (OBSERVED / "libSceNKWebKit.txt").read_text(encoding="utf-8")
        self.assertEqual(text.splitlines()[0], "# Classified: no measured use found yet.")
        self.assertEqual(progress.read_manifest(OBSERVED / "libSceNKWebKit.txt"), frozenset())
        group = progress.scan_library(PRX / "libSceNKWebKit", progress.observed_nids(PRX / "libSceNKWebKit"))
        self.assertTrue(group["classified"])
        self.assertEqual(group["todo"], 0)
        self.assertGreater(group["unobserved"], 0)

    def test_a_pending_stub_in_a_library_without_evidence_is_unobserved_and_with_evidence_observed(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(STUB.format(name="Pending") + "int APS5_VABI Ready() { return 0; }\n", encoding="utf-8")
            without = progress.scan_library(library, frozenset())
            observed = progress.scan_library(library, frozenset({progress.tokenizer.compute_nid("Pending")}))
            unclassified = progress.scan_library(library)
        self.assertEqual((without["todo"], without["unobserved"], without["done"]), (0, 1, 1))
        self.assertEqual((observed["todo"], observed["unobserved"]), (1, 0))
        self.assertEqual((unclassified["todo"], unclassified["unobserved"]), (1, 0))


if __name__ == "__main__":
    unittest.main()
