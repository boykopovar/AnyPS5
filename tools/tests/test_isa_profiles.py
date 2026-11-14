import hashlib
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import isa_profiles


def isa_xml(architecture, instructions=""):
    return (f"<Spec><ISA><Architecture><ArchitectureName>{architecture}</ArchitectureName></Architecture>"
            f"<Instructions>{instructions}</Instructions></ISA></Spec>")


def pinned(**texts):
    hashes = {key: hashlib.sha256(text.encode("utf-8")).hexdigest() for key, text in texts.items()}
    return patch.dict(isa_profiles.ISA_HASHES, hashes)


class ValidateIsaXmlTests(unittest.TestCase):
    def test_accepts_matching_architecture_and_pinned_bytes(self):
        text = isa_xml("AMD RDNA 1")
        with pinned(rdna1=text):
            root = isa_profiles.validate_isa_xml(text, "rdna1")
        self.assertEqual(root.findtext("ISA/Architecture/ArchitectureName"), "AMD RDNA 1")

    def test_rejects_any_byte_change_to_pinned_xml(self):
        text = isa_xml("AMD RDNA 1")
        with pinned(rdna1=text), self.assertRaisesRegex(ValueError, "rdna1 XML hash differs from AMD_GPU_MR_ISA_XML_2026_08_06"):
            isa_profiles.validate_isa_xml(text + "\n", "rdna1")

    def test_unknown_architecture_is_reported_before_parsing(self):
        with self.assertRaisesRegex(ValueError, "unknown ISA architecture: rdna3"):
            isa_profiles.validate_isa_xml("<not xml", "rdna3")

    def test_unparseable_xml_is_reported(self):
        with self.assertRaisesRegex(ValueError, "invalid ISA XML"):
            isa_profiles.validate_isa_xml("<Spec>", "rdna1")

    def test_wrong_architecture_is_reported_before_hash(self):
        with self.assertRaisesRegex(ValueError, "expected AMD RDNA 1 XML, got 'AMD RDNA 2'"):
            isa_profiles.validate_isa_xml(isa_xml("AMD RDNA 2"), "rdna1")

    def test_missing_architecture_name_is_reported(self):
        with self.assertRaisesRegex(ValueError, "expected AMD RDNA 1 XML, got None"):
            isa_profiles.validate_isa_xml("<Spec/>", "rdna1")


if __name__ == "__main__":
    unittest.main()
