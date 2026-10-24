import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from check_conventions import Check, named_constants, over_aligned

SOURCE = "core/libs/prx/libSceExample/tests/Example.cpp"


class OverAlignedTests(unittest.TestCase):
    def test_alignments_above_the_limit(self):
        for line, match in (
            ("alignas(65536) std::array<std::uint8_t, 4u << 20u> Texels{};", "alignas(65536)"),
            ("alignas(16384) static char block[16];", "alignas(16384)"),
            ("alignas( 0x10000 ) char block[16];", "alignas( 0x10000 )"),
            ("alignas(0X4000u) char block[16];", "alignas(0X4000u)"),
            ("alignas(65'536) char block[16];", "alignas(65'536)"),
            ("alignas(040000) char block[16];", "alignas(040000)"),
            ("alignas(0b100000000000000) char block[16];", "alignas(0b100000000000000)"),
            ("alignas(256) char small[4]; alignas(32768) char large[4];", "alignas(32768)"),
        ):
            with self.subTest(line=line):
                self.assertEqual(over_aligned(line), match)

    def test_alignments_within_the_limit(self):
        for line in (
            "alignas(8192) char block[16];",
            "alignas(0x2000) char block[16];",
            "alignas(256) std::array<std::uint32_t, 128> Input{};",
            "alignas(4096) char page[4096];",
            "alignas(std::max_align_t) char storage[16];",
            "alignas(Shader) char storage[sizeof(Shader)];",
            "realignas(65536)",
            "constexpr std::size_t TexelAlignment = 65536;",
        ):
            with self.subTest(line=line):
                self.assertIsNone(over_aligned(line))

    def test_named_constants_of_the_file(self):
        constants = named_constants("constexpr std::size_t BlockBytes = 65536;\n"
                                    "static constexpr std::uint32_t PageBytes{0x1000u};\n"
                                    "constexpr auto Large = 0x10000;\n"
                                    "constexpr std::size_t Texels = 4u << 20u;\n")
        self.assertEqual(constants, {"BlockBytes": 65536, "PageBytes": 4096, "Large": 65536})
        self.assertEqual(over_aligned("alignas(BlockBytes) std::array<std::uint8_t, BlockBytes> Block{};", constants),
                         "alignas(BlockBytes)")
        self.assertEqual(over_aligned("alignas( Large ) char block[16];", constants), "alignas( Large )")
        for line in ("alignas(PageBytes) char page[4096];", "alignas(Texels) char block[16];",
                     "alignas(inotify_event) char buffer[16384];"):
            with self.subTest(line=line):
                self.assertIsNone(over_aligned(line, constants))
        self.assertIsNone(over_aligned("alignas(BlockBytes) char block[16];"))


class AlignmentRuleTests(unittest.TestCase):
    def test_only_added_code_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(["git", "-c", f"safe.directory={root}", "-c", "user.name=Conventions Test",
                                       "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false", *args],
                                      cwd=root, capture_output=True, check=True, encoding="utf-8").stdout

            source = root / SOURCE
            source.parent.mkdir(parents=True)
            source.write_text("alignas(65536) char existing[16];\n", encoding="utf-8")
            git("init", "-q")
            git("add", ".")
            git("commit", "-qm", "test: add fixture")
            source.write_text("alignas(65536) char existing[16];\n"
                              "alignas(256) char small[16];\n"
                              "const char* text = \"alignas(65536)\";\n"
                              "alignas(0x10000) char added[16];\n"
                              "constexpr std::size_t BlockBytes = 65536;\n"
                              "alignas(BlockBytes) std::array<std::uint8_t, BlockBytes> Block{};\n", encoding="utf-8")
            git("commit", "-qam", "test: add over-aligned blocks")
            original = Path.cwd()
            try:
                os.chdir(root)
                check = Check("HEAD~1", "HEAD")
                check.code()
            finally:
                os.chdir(original)
            self.assertEqual(check.findings, [("alignment", SOURCE, 4, "alignas(0x10000)"),
                                              ("alignment", SOURCE, 6, "alignas(BlockBytes)")])


if __name__ == "__main__":
    unittest.main()
