import hashlib
import re
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
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


class ValidateFormsTests(unittest.TestCase):
    def test_well_formed_rows_are_returned_unchanged(self):
        rows = [("S_NOP", "SOPP", "base", 0), ("V_ADD_F32", "VOP2", "base", 3), ("V_ADD_F32", "VOP2", "dpp16", 3),
                ("V_MOV_B32", "VOP1", "vop3", 0x180)]
        self.assertIs(isa_profiles.validate_forms(rows), rows)

    def test_malformed_rows_are_rejected(self):
        for row in (("v_add", "VOP2", "base", 3), ("V_ADD_F32", "VOP9", "base", 3), ("V_ADD_F32", "VOP2", "vop2", 3),
                    ("V_ADD_F32", "VOP2", "base", -1), ("V_ADD_F32", "VOP2", "base", True),
                    ("V_ADD_F32", "VOP2", "base", "3")):
            with self.subTest(row=row), self.assertRaisesRegex(ValueError, "invalid ISA form"):
                isa_profiles.validate_forms([row])

    def test_modified_forms_are_only_for_vop1_vop2_and_vopc(self):
        for row in (("S_MOV_B32", "SOP1", "dpp16", 3), ("V_X", "VOP3", "vop3", 1), ("V_X", "VOP3", "dpp8", 1)):
            with self.subTest(row=row), self.assertRaisesRegex(ValueError, "invalid modified ISA form"):
                isa_profiles.validate_forms([row])

    def test_same_encoding_and_opcode_twice_is_a_duplicate(self):
        with self.assertRaisesRegex(ValueError, "duplicate ISA encoding"):
            isa_profiles.validate_forms([("V_ADD_F32", "VOP2", "base", 3), ("V_SUB_F32", "VOP2", "base", 3)])

    def test_vop3_extensions_share_the_physical_vop3_opcode_space(self):
        with self.assertRaisesRegex(ValueError, "duplicate ISA encoding"):
            isa_profiles.validate_forms([("V_MAC_LEGACY_F32", "VOP2", "vop3", 0x106), ("V_FMAC_LEGACY_F32", "VOP3", "base", 0x106)])
        with self.assertRaisesRegex(ValueError, "duplicate ISA encoding"):
            isa_profiles.validate_forms([("V_A", "VOP1", "vop3", 0x106), ("V_B", "VOP2", "vop3", 0x106)])

    def test_same_opcode_in_different_encodings_is_allowed(self):
        rows = [("A", "VOP2", "base", 0x106), ("B", "VOP3", "base", 0x106), ("C", "VOP1", "base", 0x106),
                ("D", "VOP2", "dpp16", 0x106), ("E", "VOP1", "base", 5), ("F", "VOP1", "vop3", 5)]
        self.assertIs(isa_profiles.validate_forms(rows), rows)


RDNA1 = isa_xml("AMD RDNA 1", instruction("S_MOV_B32", ("ENC_SOP1", 3)) + instruction("V_ADD_F32", ("ENC_VOP2", 3), ("ENC_VOP3", 259)))
RDNA2 = isa_xml("AMD RDNA 2", instruction("S_MOV_B32", ("ENC_SOP1", 3))
                + instruction("IMAGE_BVH_INTERSECT_RAY", ("ENC_MIMG", 230))
                + instruction("IMAGE_BVH64_INTERSECT_RAY", ("ENC_MIMG", 231))
                + instruction("V_DOT2_F32_F16", ("ENC_VOP3P", 0x1c)))


class GenerateGfx1013FormsTests(unittest.TestCase):
    def test_rdna1_supplies_base_rows_and_rdna2_only_the_bvh_rows(self):
        with pinned(rdna1=RDNA1, rdna2=RDNA2):
            forms = isa_profiles.generate_gfx1013_forms(RDNA1, RDNA2)
        self.assertEqual(forms.count(("S_MOV_B32", "SOP1", "base", 3)), 1)
        self.assertIn(("V_ADD_F32", "VOP2", "vop3", 259), forms)
        self.assertIn(("IMAGE_BVH_INTERSECT_RAY", "MIMG", "base", 230), forms)
        self.assertIn(("IMAGE_BVH64_INTERSECT_RAY", "MIMG", "base", 231), forms)
        self.assertNotIn("V_DOT2_F32_F16", {name for name, _, _, _ in forms})

    def test_output_is_sorted_and_includes_the_supplemental_table(self):
        with pinned(rdna1=RDNA1, rdna2=RDNA2):
            forms = isa_profiles.generate_gfx1013_forms(RDNA1, RDNA2)
        self.assertEqual(forms, sorted(forms))
        self.assertIn(("S_STORE_DWORD", "SMEM", "base", 16), forms)
        self.assertIn(("V_MUL_LO_I32", "VOP3", "base", 363), forms)

    def test_identical_xml_row_and_supplemental_row_are_merged(self):
        rdna1 = isa_xml("AMD RDNA 1", instruction("S_STORE_DWORD", ("ENC_SMEM", 16)))
        with pinned(rdna1=rdna1, rdna2=RDNA2):
            forms = isa_profiles.generate_gfx1013_forms(rdna1, RDNA2)
        self.assertEqual(forms.count(("S_STORE_DWORD", "SMEM", "base", 16)), 1)

    def test_different_name_on_a_supplemental_opcode_is_rejected(self):
        rdna1 = isa_xml("AMD RDNA 1", instruction("S_FOO", ("ENC_SMEM", 16)))
        with pinned(rdna1=rdna1, rdna2=RDNA2), self.assertRaisesRegex(ValueError, "duplicate ISA encoding"):
            isa_profiles.generate_gfx1013_forms(rdna1, RDNA2)

    def test_architecture_of_each_input_is_checked(self):
        with self.assertRaisesRegex(ValueError, "expected AMD RDNA 1 XML, got 'AMD RDNA 2'"):
            isa_profiles.generate_gfx1013_forms(RDNA2, RDNA2)
        with pinned(rdna1=RDNA1), self.assertRaisesRegex(ValueError, "expected AMD RDNA 2 XML, got 'AMD RDNA 1'"):
            isa_profiles.generate_gfx1013_forms(RDNA1, RDNA1)


class LlvmSupplementalFormsTests(unittest.TestCase):
    def test_table_has_distinct_rows_that_pass_validation(self):
        rows = isa_profiles.llvm_gfx1013_additional_forms()
        self.assertEqual(len(rows), 96)
        self.assertEqual(len(set(rows)), len(rows))
        self.assertEqual(isa_profiles.validate_forms(list(rows)), list(rows))

    def test_scalar_store_atomic_and_cache_opcodes(self):
        rows = set(isa_profiles.llvm_gfx1013_additional_forms())
        self.assertIn(("S_STORE_DWORD", "SMEM", "base", 0x10), rows)
        self.assertIn(("S_STORE_DWORDX4", "SMEM", "base", 0x12), rows)
        self.assertIn(("S_SCRATCH_STORE_DWORDX2", "SMEM", "base", 0x16), rows)
        self.assertIn(("S_BUFFER_STORE_DWORDX4", "SMEM", "base", 0x1a), rows)
        self.assertIn(("S_BUFFER_ATOMIC_CMPSWAP", "SMEM", "base", 0x41), rows)
        self.assertIn(("S_BUFFER_ATOMIC_CMPSWAP_X2", "SMEM", "base", 0x61), rows)
        self.assertIn(("S_ATOMIC_ADD", "SMEM", "base", 0x82), rows)
        self.assertIn(("S_ATOMIC_ADD_X2", "SMEM", "base", 0xa2), rows)
        self.assertIn(("S_DCACHE_WB", "SMEM", "base", 0x21), rows)
        self.assertIn(("S_DCACHE_DISCARD_X2", "SMEM", "base", 0x29), rows)

    def test_ds_source2_opcodes_cover_both_widths(self):
        rows = set(isa_profiles.llvm_gfx1013_additional_forms())
        self.assertIn(("DS_ADD_SRC2_U32", "DS", "base", 0x80), rows)
        self.assertIn(("DS_RSUB_SRC2_U32", "DS", "base", 0x82), rows)
        self.assertIn(("DS_WRITE_SRC2_B32", "DS", "base", 0x8d), rows)
        self.assertIn(("DS_MIN_SRC2_F32", "DS", "base", 0x92), rows)
        self.assertIn(("DS_ADD_SRC2_F32", "DS", "base", 0x95), rows)
        self.assertIn(("DS_ADD_SRC2_U64", "DS", "base", 0xc0), rows)
        self.assertIn(("DS_MAX_SRC2_F64", "DS", "base", 0xd3), rows)
        self.assertIn(("V_MUL_LO_I32", "VOP3", "base", 0x16b), rows)


class LoadGfx1013FormsTests(unittest.TestCase):
    def test_default_path_is_the_committed_inventory(self):
        self.assertEqual(isa_profiles.load_gfx1013_forms(), isa_profiles.load_gfx1013_forms(TOOLS / "gfx1013_isa_forms.txt"))

    def test_blank_and_comment_lines_are_skipped_and_file_order_is_kept(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "forms.txt"
            path.write_text("\n  # indented comment\n# comment\nV_B VOP2 base 4\n\nS_A SOP1 base 3\n", encoding="utf-8")
            self.assertEqual(isa_profiles.load_gfx1013_forms(path), [("V_B", "VOP2", "base", 4), ("S_A", "SOP1", "base", 3)])

    def test_invalid_line_is_reported_with_its_physical_line_number(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "forms.txt"
            path.write_text("# comment\n\nS_A SOP1 base 3\nS_B SOP1 base\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, f"invalid ISA form at {re.escape(str(path))}:4"):
                isa_profiles.load_gfx1013_forms(path)

    def test_non_numeric_opcode_and_extra_fields_are_invalid_lines(self):
        for text in ("S_A SOP1 base x\n", "S_A SOP1 base 3 extra\n"):
            with self.subTest(text=text), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "forms.txt"
                path.write_text(text, encoding="utf-8")
                with self.assertRaisesRegex(ValueError, f"invalid ISA form at {re.escape(str(path))}:1"):
                    isa_profiles.load_gfx1013_forms(path)

    def test_file_without_forms_is_an_error(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "forms.txt"
            path.write_text("# only comments\n\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, f"no ISA forms in {re.escape(str(path))}"):
                isa_profiles.load_gfx1013_forms(path)

    def test_loaded_rows_are_validated(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "forms.txt"
            path.write_text("S_A SOP1 base 3\nS_B SOP1 base 3\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate ISA encoding"):
                isa_profiles.load_gfx1013_forms(path)

    def test_missing_file_is_not_an_empty_inventory(self):
        with tempfile.TemporaryDirectory() as directory, self.assertRaises(FileNotFoundError):
            isa_profiles.load_gfx1013_forms(Path(directory) / "missing.txt")


class InventoryTextTests(unittest.TestCase):
    def test_header_is_comment_only_and_names_the_pinned_sources(self):
        header = isa_profiles.gfx1013_forms_header()
        self.assertTrue(header.endswith("\n"))
        self.assertTrue(all(line.startswith("#") for line in header.splitlines()))
        self.assertIn(isa_profiles.ISA_RELEASE, header)
        self.assertIn(isa_profiles.ISA_HASHES["rdna1"], header)
        self.assertIn(isa_profiles.ISA_HASHES["rdna2"], header)
        self.assertIn(isa_profiles.LLVM_TAG, header)

    def test_header_alone_loads_as_no_forms(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "forms.txt"
            path.write_text(isa_profiles.gfx1013_forms_header(), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, f"no ISA forms in {re.escape(str(path))}"):
                isa_profiles.load_gfx1013_forms(path)

    def test_committed_inventory_starts_with_the_generated_header(self):
        committed = (TOOLS / "gfx1013_isa_forms.txt").read_text(encoding="utf-8")
        self.assertTrue(committed.startswith(isa_profiles.gfx1013_forms_header()))

    def test_text_is_the_header_followed_by_one_row_per_form(self):
        with pinned(rdna1=RDNA1, rdna2=RDNA2):
            text = isa_profiles.gfx1013_forms_text(RDNA1, RDNA2)
            forms = isa_profiles.generate_gfx1013_forms(RDNA1, RDNA2)
            header = isa_profiles.gfx1013_forms_header()
        self.assertTrue(text.startswith(header))
        rows = "".join(f"{name} {encoding} {form} {opcode}\n" for name, encoding, form, opcode in forms)
        self.assertEqual(text, header + rows)
        self.assertIn("S_MOV_B32 SOP1 base 3\n", text)
        self.assertIn("V_ADD_F32 VOP2 vop3 259\n", text)

    def test_text_loads_back_to_the_generated_forms(self):
        with pinned(rdna1=RDNA1, rdna2=RDNA2), tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "forms.txt"
            path.write_text(isa_profiles.gfx1013_forms_text(RDNA1, RDNA2), encoding="utf-8")
            loaded = isa_profiles.load_gfx1013_forms(path)
            self.assertEqual(loaded, isa_profiles.generate_gfx1013_forms(RDNA1, RDNA2))

    def test_write_stores_the_utf8_text_bytes(self):
        with pinned(rdna1=RDNA1, rdna2=RDNA2), tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "forms.txt"
            isa_profiles.write_gfx1013_forms(path, RDNA1, RDNA2)
            self.assertEqual(path.read_bytes(), isa_profiles.gfx1013_forms_text(RDNA1, RDNA2).encode("utf-8"))

    def test_failed_generation_leaves_the_target_untouched(self):
        with tempfile.TemporaryDirectory() as directory:
            existing = Path(directory) / "existing.txt"
            existing.write_bytes(b"old\n")
            absent = Path(directory) / "absent.txt"
            with pinned(rdna1=RDNA1, rdna2=RDNA2):
                for target in (existing, absent):
                    with self.subTest(target=target.name), self.assertRaisesRegex(ValueError, "expected AMD RDNA 2 XML"):
                        isa_profiles.write_gfx1013_forms(target, RDNA1, RDNA1)
            self.assertEqual(existing.read_bytes(), b"old\n")
            self.assertFalse(absent.exists())


if __name__ == "__main__":
    unittest.main()
