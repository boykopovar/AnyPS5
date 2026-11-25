import io
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_linux_release_abi


def make_archive(path, files):
    with tarfile.open(path, "w:gz") as archive:
        for name, data in files.items():
            member = tarfile.TarInfo(name)
            member.size = len(data)
            archive.addfile(member, io.BytesIO(data))


class Tests(unittest.TestCase):
    def test_limits_use_requirements_and_accept_existing_abi_variants(self):
        listing = """Version definition section '.gnu.version_d' contains 1 entry:
  0x0030: Rev: 1 Flags: none Index: 2 Cnt: 1 Name: GLIBCXX_3.4.99
Version needs section '.gnu.version_r' contains 2 entries:
  000000: Version: 1 File: libm.so.6 Cnt: 4
  0x0010: Name: GLIBC_2.9 Flags: none Version: 2
  0x0020: Name: GLIBC_2.38 Flags: none Version: 3
  0x0030: Name: GLIBC_ABI_DT_RELR Flags: none Version: 4
  0x0040: Name: GLIBC_2.43 Flags: none Version: 5
  0x0050: Version: 1 File: libstdc++.so.6 Cnt: 3
  0x0060: Name: GLIBCXX_3.4.31 Flags: none Version: 6
  0x0070: Name: GLIBCXX_3.4.32 Flags: none Version: 7
  0x0080: Name: CXXABI_1.3.9 Flags: none Version: 8
"""
        self.assertEqual(list(check_linux_release_abi.incompatible_versions(listing)), [
            "requires GLIBC_2.43 from libm.so.6 (release baseline: GLIBC_2.38)",
            "requires GLIBCXX_3.4.32 from libstdc++.so.6 (release baseline: GLIBCXX_3.4.31)",
        ])

    def test_direct_runtime_entries_include_optional_shared_libraries(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            bundle = root / "prx-linux.tar.gz"
            make_archive(bundle, {
                "libs/libc.prx": b"guest runtime",
                "libs/libmimalloc.so": b"allocator",
                "libs/libTracyClient.so.0.11.1": b"tracy",
                "libs/licenses/libc.prx": b"licence outside runtime directory",
            })
            selected = []

            def read_elf(path):
                selected.append(path.read_bytes())
                return "No version information found in this file."

            with patch.object(check_linux_release_abi, "read_version_info", side_effect=read_elf):
                self.assertEqual(check_linux_release_abi.check_archive(bundle, root / "runtime.elf"), (3, []))
            self.assertEqual(selected, [b"guest runtime", b"allocator", b"tracy"])

    def test_no_runtime_libraries_is_an_error(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            bundle = root / "prx-linux.tar.gz"
            make_archive(bundle, {"libs/licenses/libc.txt": b"licence"})
            with self.assertRaisesRegex(ValueError, "no runtime libraries"):
                check_linux_release_abi.check_archive(bundle, root / "runtime.elf")

    @unittest.skipUnless(
        sys.platform == "linux" and platform.machine() == "x86_64" and
        all(shutil.which(tool) for tool in ("as", "ld", "readelf")),
        "requires Linux x86-64 GNU binutils",
    )
    def test_real_elf_detects_the_newer_runtime_requirement(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def run(*command):
                return subprocess.run(command, cwd=root, check=True, capture_output=True, timeout=20)

            (root / "provides.s").write_text(".globl sample\nsample:\n ret\n", encoding="utf-8")
            (root / "uses.s").write_text(".globl use_sample\nuse_sample:\n jmp sample@PLT\n", encoding="utf-8")
            (root / "versions.map").write_text("GLIBC_2.43 { global: sample; local: *; };\n", encoding="utf-8")
            run("as", "-o", "provides.o", "provides.s")
            run("as", "-o", "uses.o", "uses.s")
            run("ld", "-shared", "-soname", "libexample.so", "--version-script=versions.map",
                "-o", "libexample.so", "provides.o")
            run("ld", "-shared", "-o", "uses.prx", "uses.o", "./libexample.so")
            compatible = root / "prx-linux-v1.tar.gz"
            incompatible = root / "prx-linux-unpatched-v1.tar.gz"
            make_archive(compatible, {"libs/libexample.prx": (root / "libexample.so").read_bytes()})
            make_archive(incompatible, {"libs/libexample.prx": (root / "uses.prx").read_bytes()})
            tool = Path(check_linux_release_abi.__file__).resolve()
            self.assertEqual(run(sys.executable, str(tool), str(compatible)).returncode, 0)
            result = subprocess.run(
                [sys.executable, str(tool), str(compatible), str(incompatible)],
                capture_output=True, text=True, timeout=20,
            )
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertIn(f"{incompatible}:libs/libexample.prx: requires GLIBC_2.43 from libexample.so", result.stderr)
            self.assertEqual(result.stdout.count("checked 1 runtime libraries"), 2)


if __name__ == "__main__":
    unittest.main()
