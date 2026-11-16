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


if __name__ == "__main__":
    unittest.main()
