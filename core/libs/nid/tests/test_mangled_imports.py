import base64
import hashlib
import sys
import unittest

import test_symbol_versions as symbols_module
from test_symbol_versions import build_image, patch, read_dynsym, DEFINITIONS, SHN_UNDEF, STT_FUNC, PATCHER

SCE_IMPORTS = ["_ZN3sce4Json5Value3setEl", "_ZNK3sce4Json5Value3getEv", "_ZN3Sce4Json6StringC1EPKc"]
OTHER_IMPORTS = ["_ZN4demo3FooEv", "_ZN5scene3BarEv", "_ZNSt6vectorIiE4sizeEv", "_ZN3scx3BazEv", "_Z3sce"]
PLAIN_IMPORT = "sceKernelPlainImportName"


def nid(name):
    digest = hashlib.sha1(name.encode() + bytes.fromhex("518d64a635ded8c1e6b039b1c3e55230")).digest()
    return base64.b64encode(digest[:8][::-1]).decode().rstrip("=").replace("/", "-")


def image(imports):
    exports = DEFINITIONS[:10 - 1 - len(imports)]
    entries = [(None, 1, SHN_UNDEF, 0, 0)]
    entries += [(name, 1, SHN_UNDEF, 0, 0) for name in imports]
    entries += [(name, 1, 9, STT_FUNC, value) for name, _, value in exports]
    symbols_module.HASHED_FROM = 1 + len(imports)
    return bytes(build_image(entries, False))


@unittest.skipUnless(PATCHER is not None and PATCHER.is_file(), "nid_patcher executable required")
class MangledImportTests(unittest.TestCase):
    def test_library_rewrites_mangled_sce_imports_like_the_export_side(self):
        result, after = patch(PATCHER, image(SCE_IMPORTS + [PLAIN_IMPORT]), "libimporter.so")
        self.assertEqual(result.returncode, 0, result.stderr)
        undefined = [entry[0] for entry in read_dynsym(after) if entry[1] == SHN_UNDEF and entry[0]]
        for name in SCE_IMPORTS + [PLAIN_IMPORT]:
            with self.subTest(name=name):
                self.assertIn(nid(name), undefined)
                self.assertNotIn(name, undefined)

    def test_library_keeps_other_mangled_imports(self):
        result, after = patch(PATCHER, image(OTHER_IMPORTS), "libimporter.so")
        self.assertEqual(result.returncode, 0, result.stderr)
        undefined = [entry[0] for entry in read_dynsym(after) if entry[1] == SHN_UNDEF and entry[0]]
        for name in OTHER_IMPORTS:
            with self.subTest(name=name):
                self.assertIn(name, undefined)
                self.assertNotIn(nid(name), undefined)

    def test_exe_only_sce_keeps_mangled_imports(self):
        import subprocess, tempfile
        from pathlib import Path
        with tempfile.TemporaryDirectory(prefix="anyps5-mangled-") as directory:
            path = Path(directory) / "importer"
            path.write_bytes(image(SCE_IMPORTS + [PLAIN_IMPORT]))
            result = subprocess.run([PATCHER, "--exec", "--only-sce", str(path)], capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            undefined = [entry[0] for entry in read_dynsym(path.read_bytes()) if entry[1] == SHN_UNDEF and entry[0]]
            for name in SCE_IMPORTS:
                self.assertIn(name, undefined)
            self.assertIn(nid(PLAIN_IMPORT), undefined)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
