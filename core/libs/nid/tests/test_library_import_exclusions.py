import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_symbol_versions import build_image, read_dynsym, PATCHER, SHN_UNDEF, STT_FUNC


def image(imports, exports):
    entries = [(None, 1, SHN_UNDEF, 0, 0)]
    entries += [(name, 1, SHN_UNDEF, 0, 0) for name in imports]
    entries += [(name, 1, 9, STT_FUNC, 0x1200 + index * 0x20) for index, name in enumerate(exports)]
    return bytes(build_image(entries, False))


@unittest.skipUnless(PATCHER is not None and PATCHER.is_file(), "nid_patcher executable required")
class LibraryImportExclusionTests(unittest.TestCase):
    def patched_names(self, args, imports, exports, excluded):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "case.so"
            path.write_bytes(image(imports, exports))
            listing = root / "keep.txt"
            listing.write_bytes((excluded + "\n").encode())
            result = subprocess.run([str(PATCHER), *args, "--preserve-export-list", str(listing), str(path)],
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            return {entry[0] for entry in read_dynsym(path.read_bytes()) if entry[0]}

    def test_excluded_sce_import_keeps_its_name_in_library_mode(self):
        names = self.patched_names(["libtest"], ["sceKernelFoo"], ["sceBarBazQuxLongName"], "sceKernelFoo")
        self.assertIn("sceKernelFoo", names)

    def test_non_excluded_sce_import_is_renamed_in_library_mode(self):
        names = self.patched_names(["libtest"], ["sceOtherLongImportName"], ["sceKernelFoo"], "sceKernelFoo")
        self.assertNotIn("sceOtherLongImportName", names)

    def test_excluded_sce_import_keeps_its_name_in_executable_mode(self):
        names = self.patched_names(["--exec", "--only-sce"], ["sceKernelFoo"], ["sceBarBazQuxLongName"], "sceKernelFoo")
        self.assertIn("sceKernelFoo", names)


if __name__ == "__main__":
    unittest.main()
