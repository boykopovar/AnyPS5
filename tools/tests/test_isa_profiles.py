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


class CanonicalEncodingTests(unittest.TestCase):
    def test_literal_suffix_names_the_base_encoding(self):
        self.assertEqual(isa_profiles.canonical_encoding("VOP2_INST_LITERAL", None), ("VOP2", "base"))
        self.assertEqual(isa_profiles.canonical_encoding("FLAT_GLBL_INST_LITERAL", None), ("GLOBAL", "base"))
        self.assertEqual(isa_profiles.canonical_encoding("SOP2_INST_LITERAL", None), ("SOP2", "base"))

    def test_vop3_extends_vop1_vop2_and_vopc_only(self):
        self.assertEqual(isa_profiles.canonical_encoding("ENC_VOP3", "VOP1"), ("VOP1", "vop3"))
        self.assertEqual(isa_profiles.canonical_encoding("ENC_VOP3", "VOP2"), ("VOP2", "vop3"))
        self.assertEqual(isa_profiles.canonical_encoding("ENC_VOP3", "VOPC"), ("VOPC", "vop3"))
        self.assertEqual(isa_profiles.canonical_encoding("ENC_VOP3", "SOP2"), ("VOP3", "base"))
        self.assertEqual(isa_profiles.canonical_encoding("ENC_VOP3", None), ("VOP3", "base"))

    def test_vop3_sdst_and_literal_vop3_follow_the_same_rule(self):
        self.assertEqual(isa_profiles.canonical_encoding("VOP3_SDST_ENC", "VOP2"), ("VOP2", "vop3"))
        self.assertEqual(isa_profiles.canonical_encoding("VOP3_INST_LITERAL", "VOP1"), ("VOP1", "vop3"))
        self.assertEqual(isa_profiles.canonical_encoding("VOP3_SDST_ENC_INST_LITERAL", None), ("VOP3", "base"))

    def test_flat_global_scratch_and_flat_names(self):
        self.assertEqual(isa_profiles.canonical_encoding("ENC_FLAT_GLBL", None), ("GLOBAL", "base"))
        self.assertEqual(isa_profiles.canonical_encoding("ENC_FLAT_SCRATCH", None), ("SCRATCH", "base"))
        self.assertEqual(isa_profiles.canonical_encoding("ENC_FLAT", None), ("FLAT", "base"))

    def test_dpp_variants_of_vop1_vop2_and_vopc(self):
        self.assertEqual(isa_profiles.canonical_encoding("VOP1_VOP_DPP16", None), ("VOP1", "dpp16"))
        self.assertEqual(isa_profiles.canonical_encoding("VOP2_VOP_DPP8", None), ("VOP2", "dpp8"))
        self.assertEqual(isa_profiles.canonical_encoding("VOPC_VOP_DPP16", None), ("VOPC", "dpp16"))

    def test_other_tags_are_not_forms(self):
        self.assertIsNone(isa_profiles.canonical_encoding("NOT_A_FORM", None))
        self.assertIsNone(isa_profiles.canonical_encoding("SOP1_VOP_DPP16", None))
        self.assertIsNone(isa_profiles.canonical_encoding("VOP3_VOP_DPP8", None))


if __name__ == "__main__":
    unittest.main()
