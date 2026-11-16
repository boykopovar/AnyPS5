import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import isa_profiles


class IsaProfilesTests(unittest.TestCase):
    def test_canonical_encoding_maps_llvm_tags(self):
        self.assertEqual(isa_profiles.canonical_encoding("ENC_FLAT_GLBL", "VOP2"), ("GLOBAL", "base"))
        self.assertEqual(isa_profiles.canonical_encoding("ENC_FLAT_SCRATCH", "VOP2"), ("SCRATCH", "base"))
        self.assertEqual(isa_profiles.canonical_encoding("ENC_VOP3", "VOP2"), ("VOP2", "vop3"))
        self.assertEqual(isa_profiles.canonical_encoding("ENC_VOP3", "SOP1"), ("VOP3", "base"))
        self.assertEqual(isa_profiles.canonical_encoding("VOP2_VOP_DPP16", "VOP2"), ("VOP2", "dpp16"))
        self.assertIsNone(isa_profiles.canonical_encoding("NOT_AN_ENCODING", "VOP2"))

    def test_validate_forms_accepts_known_rows(self):
        encoding = next(iter(sorted(isa_profiles.ENCODINGS)))
        form = isa_profiles.FORMS[0]
        isa_profiles.validate_forms([("V_ADD_F32", encoding, form, 3)])

    def test_validate_forms_rejects_malformed_rows(self):
        encoding = next(iter(sorted(isa_profiles.ENCODINGS)))
        form = isa_profiles.FORMS[0]
        for row in (("v_add", encoding, form, 3), ("V_ADD", encoding, "not_a_form", 3),
                    ("V_ADD", encoding, form, -1), ("V_ADD", encoding, form, "3")):
            with self.assertRaises(ValueError, msg=repr(row)):
                isa_profiles.validate_forms([row])

    def test_validate_isa_xml_refuses_unknown_architecture_and_bad_xml(self):
        with self.assertRaisesRegex(ValueError, "unknown ISA architecture"):
            isa_profiles.validate_isa_xml("<ISA/>", "rdna9")
        with self.assertRaisesRegex(ValueError, "invalid ISA XML"):
            isa_profiles.validate_isa_xml("<ISA>", "rdna1")


if __name__ == "__main__":
    unittest.main()
