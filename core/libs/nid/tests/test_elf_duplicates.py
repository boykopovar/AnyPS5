import base64
import hashlib
import sys
import unittest
from pathlib import Path

from test_symbol_versions import build_image, patch, read_dynsym, IMPORTS, NID_NAME, SHN_UNDEF, STT_FUNC


def nid(name):
    digest = hashlib.sha1(name.encode() + bytes.fromhex("518d64a635ded8c1e6b039b1c3e55230")).digest()
    return base64.b64encode(digest[:8][::-1]).decode().rstrip("=").replace("/", "-")


def image(names, addresses=None):
    if addresses is None:
        addresses = [0x1200 + index * 0x20 for index in range(len(names))]
    entries = [(None, 1, SHN_UNDEF, 0, 0)]
    entries += [(name, 1, SHN_UNDEF, 0, 0) for name in IMPORTS]
    entries += [(name, 1, 9, STT_FUNC, address) for name, address in zip(names, addresses)]
    return bytes(build_image(entries, False))


class ElfDuplicateTests(unittest.TestCase):
    def test_exports_patched_to_one_name_are_rejected(self):
        cases = (
            ["sceAlphaComputeLength", "sceAlphaComputeLength_nid_disambig1", "sceBetaCopyString"],
            ["sceAlphaComputeLength_nid_postfix", "sceAlphaComputeLength_nid_disambig12", "sceBetaCopyString"],
            ["sceBetaCopyString", "sceAlphaComputeLength_nid_no_patch", "sceAlphaComputeLength_nid_no_patch_nid_no_patch_cut"],
        )
        for names in cases:
            with self.subTest(names=names):
                before = image(names)
                result, after = patch(PATCHER, before, "duplicates.so")
                self.assertEqual(result.returncode, 2, (result.returncode, result.stderr))
                self.assertIn(b"duplicate patched export name", result.stderr)
                self.assertEqual(after, before)

    def test_aliases_of_one_address_are_patched(self):
        names = ["sceAlphaComputeLength", "sceAlphaComputeLength_nid_postfix", "sceAlphaComputeLength_nid_no_patch_cut", "sceBetaCopyString"]
        result, after = patch(PATCHER, image(names, [0x1200, 0x1200, 0x1200, 0x1220]), "aliases.so")
        self.assertEqual(result.returncode, 0, result.stderr)
        patched = [(entry[0], entry[2]) for entry in read_dynsym(after) if entry[1] == 9]
        self.assertEqual(sorted(name for name, address in patched if address == 0x1200),
                         sorted(["sceAlphaComputeLength", "sceAlphaComputeLength", nid("sceAlphaComputeLength")]))
        self.assertEqual([name for name, address in patched if address == 0x1220], [nid("sceBetaCopyString")])
        self.assertTrue(NID_NAME.match(nid("sceAlphaComputeLength")))

    def test_aliases_of_different_addresses_are_rejected(self):
        names = ["sceAlphaComputeLength", "sceAlphaComputeLength_nid_postfix", "sceAlphaComputeLength_nid_no_patch_cut"]
        before = image(names, [0x1200, 0x1200, 0x1220])
        result, after = patch(PATCHER, before, "aliases.so")
        self.assertEqual(result.returncode, 2, (result.returncode, result.stderr))
        self.assertIn(b"duplicate patched export name: sceAlphaComputeLength", result.stderr)
        self.assertEqual(after, before)

    def test_distinct_exports_are_still_patched(self):
        names = ["sceAlphaComputeLength_nid_disambig1", "sceBetaCopyString_nid_postfix", "sceGammaKeep_nid_no_patch_cut"]
        result, after = patch(PATCHER, image(names), "distinct.so")
        self.assertEqual(result.returncode, 0, result.stderr)
        patched = [entry[0] for entry in read_dynsym(after) if entry[1] == 9]
        self.assertEqual(len(patched), 3)
        self.assertEqual(len(set(patched)), 3)
        self.assertIn("sceGammaKeep", patched)


if __name__ == "__main__":
    PATCHER = Path(sys.argv.pop(1)).resolve()
    unittest.main()
