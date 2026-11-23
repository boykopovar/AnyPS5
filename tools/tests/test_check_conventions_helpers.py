import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_conventions


class HelperTests(unittest.TestCase):
    def test_symbol_nid_computes_the_nid_of_a_plain_export(self):
        self.assertEqual(check_conventions.symbol_nid("sceFoo_nid_postfix"),
                         check_conventions.compute_nid("sceFoo"))
        self.assertEqual(check_conventions.symbol_nid("sceFoo_nid_disambig2_nid_postfix"),
                         check_conventions.compute_nid("sceFoo"))

    def test_symbol_nid_skips_unpatched_and_non_export_names(self):
        self.assertIsNone(check_conventions.symbol_nid("sceFoo_nid_no_patch"))
        self.assertIsNone(check_conventions.symbol_nid("sceFoo_nid_no_patch_cut"))
        self.assertIsNone(check_conventions.symbol_nid("SDL_Init"))
        self.assertIsNone(check_conventions.symbol_nid("_nid_postfix"))

    def test_is_debt_accepts_only_top_level_debt_files(self):
        self.assertTrue(check_conventions.is_debt(check_conventions.DEBT))
        self.assertTrue(check_conventions.is_debt("docs/dev/debt/libSceFoo.md"))
        self.assertFalse(check_conventions.is_debt("docs/dev/debt/changes/x.md"))
        self.assertFalse(check_conventions.is_debt("docs/dev/debt/sub/x.md"))
        self.assertFalse(check_conventions.is_debt("docs/other.md"))

    def test_is_debt_fragment_accepts_only_direct_fragments(self):
        self.assertTrue(check_conventions.is_debt_fragment("docs/dev/debt/changes/x.md"))
        self.assertFalse(check_conventions.is_debt_fragment("docs/dev/debt/changes/sub/x.md"))
        self.assertFalse(check_conventions.is_debt_fragment("docs/dev/debt/libSceFoo.md"))

    def test_link_bases_resolves_fragment_links_as_if_in_the_debt_folder(self):
        self.assertEqual(set(check_conventions.link_bases("docs/dev/debt/changes/x.md", "[a](a.md)\n").values()),
                         {f"{check_conventions.DEBT_DIR}fragment.md"})
        self.assertEqual(set(check_conventions.link_bases("docs/dev/BUILD.md", "text\n").values()), {"docs/dev/BUILD.md"})

    def test_utf8_accepts_valid_utf8_and_refuses_invalid_bytes(self):
        self.assertTrue(check_conventions.utf8("ñ".encode("utf-8")))
        self.assertFalse(check_conventions.utf8(b"\xff\xfe"))


if __name__ == "__main__":
    unittest.main()
