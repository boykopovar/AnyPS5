import hashlib
import sys
import unittest
import xml.etree.ElementTree as ET
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


def instruction(name, *encodings):
    items = "".join(f"<InstructionEncoding><EncodingName>{tag}</EncodingName><Opcode Radix=\"10\">{opcode}</Opcode>"
                    "</InstructionEncoding>" for tag, opcode in encodings)
    return f"<Instruction><InstructionName>{name}</InstructionName><InstructionEncodings>{items}</InstructionEncodings></Instruction>"


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


class CanonicalXmlFormsTests(unittest.TestCase):
    def forms(self, *instructions):
        return isa_profiles.canonical_xml_forms(ET.fromstring(isa_xml("AMD RDNA 1", "".join(instructions))))

    def test_one_instruction_yields_base_vop3_and_modified_rows(self):
        forms = self.forms(instruction("V_ADD_F32", ("ENC_VOP2", 3), ("ENC_VOP3", 259),
                                       ("VOP2_VOP_DPP16", 3), ("VOP2_VOP_DPP8", 3)))
        self.assertEqual(forms, {("V_ADD_F32", "VOP2", "base", 3), ("V_ADD_F32", "VOP2", "vop3", 259),
                                 ("V_ADD_F32", "VOP2", "dpp16", 3), ("V_ADD_F32", "VOP2", "dpp8", 3)})

    def test_vop3_without_a_vop12c_sibling_is_vop3_base(self):
        self.assertEqual(self.forms(instruction("V_MUL_LO_I32", ("ENC_VOP3", 0x16b))),
                         {("V_MUL_LO_I32", "VOP3", "base", 0x16b)})

    def test_repeated_literal_positions_collapse_to_one_row(self):
        forms = self.forms(instruction("V_FMAMK_F32", ("VOP2_INST_LITERAL", 44), ("VOP2_INST_LITERAL", 44)))
        self.assertEqual(forms, {("V_FMAMK_F32", "VOP2", "base", 44)})

    def test_unknown_encoding_tags_are_skipped(self):
        forms = self.forms(instruction("S_NOP", ("ENC_SOPP", 0), ("UNKNOWN_TAG", 1)))
        self.assertEqual(forms, {("S_NOP", "SOPP", "base", 0)})

    def test_missing_instruction_table_is_an_error(self):
        with self.assertRaisesRegex(ValueError, "ISA XML has no instructions"):
            isa_profiles.canonical_xml_forms(ET.fromstring("<Spec><ISA/></Spec>"))

    def test_empty_instruction_table_yields_no_forms(self):
        self.assertEqual(self.forms(), set())


if __name__ == "__main__":
    unittest.main()
