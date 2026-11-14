import itertools
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from check_conventions import Check, cpp_text, named_constants, over_aligned

SOURCE = "core/libs/prx/libSceExample/tests/Example.cpp"

SMALL = "void Small() {\n    constexpr std::size_t Alignment = 64;\n    alignas(Alignment) static char small[4];\n}\n"
LARGE = "void Large() {\n    constexpr std::size_t Alignment = 16384;\n    alignas(Alignment) static char large[4];\n}\n"


def reported(text):
    constants = named_constants(cpp_text(text))
    lines = text.split("\n")
    starts = list(itertools.accumulate((len(line) + 1 for line in lines), initial=0))
    return [number for number, line in enumerate(lines, 1) if over_aligned(line, constants, starts[number - 1])]


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
        self.assertEqual(reported("constexpr std::size_t BlockBytes = 65536;\n"
                                  "static constexpr std::uint32_t PageBytes{0x1000u};\n"
                                  "constexpr auto Large = 0x10000;\n"
                                  "constexpr std::size_t Texels = 4u << 20u;\n"
                                  "alignas(BlockBytes) std::array<std::uint8_t, BlockBytes> Block{};\n"
                                  "alignas( Large ) char block[16];\n"
                                  "alignas(PageBytes) char page[4096];\n"
                                  "alignas(Texels) char texels[16];\n"
                                  "alignas(inotify_event) char buffer[16384];\n"), [5, 6])
        self.assertEqual(reported("alignas(BlockBytes) char block[16];\n"
                                  "constexpr std::size_t BlockBytes = 65536;\n"), [])

    def test_constants_in_separate_scopes(self):
        self.assertEqual(reported(SMALL + LARGE), [7])
        self.assertEqual(reported(LARGE + SMALL), [3])

    def test_constants_in_nested_scopes(self):
        for outer, inner, expected in ((64, 16384, [6]), (16384, 64, [3, 8])):
            text = ("void Nested() {\n"
                    f"    constexpr std::size_t Alignment = {outer};\n"
                    "    alignas(Alignment) static char before[4];\n"
                    "    {\n"
                    f"        constexpr std::size_t Alignment = {inner};\n"
                    "        alignas(Alignment) static char inside[4];\n"
                    "    }\n"
                    "    alignas(Alignment) static char after[4];\n"
                    "}\n")
            with self.subTest(outer=outer, inner=inner):
                self.assertEqual(reported(text), expected)

    def test_constants_in_reopened_namespaces(self):
        large = "constexpr std::size_t Alignment = 16384;"
        use = "alignas(Alignment) static char data[4];"
        for text, expected in (
            (f"namespace N {{ {large} }}\nnamespace N {{ {use} }}\n", [2]),
            (f"namespace N {{ {large} }}\nnamespace M {{ {use} }}\n", []),
            (f"namespace {{ {large} }}\nnamespace {{ {use} }}\n", [2]),
            (f"namespace A::B {{ {large} }}\nnamespace A {{ namespace B {{ {use} }} }}\n", [2]),
            (f"namespace A {{ {large} }}\nnamespace A::B {{ {use} }}\n", [2]),
            (f"namespace A {{ inline namespace V1 {{ {large} }} }}\nnamespace A {{ {use} }}\n", [2]),
            (f"namespace A {{ inline namespace V1 {{ {large} }} }}\nnamespace B {{ {use} }}\n", []),
            (f"namespace N {{ {large} }}\nnamespace N {{ void F() {{ constexpr std::size_t Alignment = 64; {use} }} }}\n", []),
            (f'extern "C" {{ {large} }}\n{use}\n', [2]),
        ):
            with self.subTest(text=text):
                self.assertEqual(reported(text), expected)


class AlignmentRuleTests(unittest.TestCase):
    def findings(self, after):
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
            source.write_text("alignas(65536) char existing[16];\n" + after, encoding="utf-8")
            git("commit", "-qam", "test: add over-aligned blocks")
            original = Path.cwd()
            try:
                os.chdir(root)
                check = Check("HEAD~1", "HEAD")
                check.code()
            finally:
                os.chdir(original)
            return check.findings

    def test_abi_alias_expansion_does_not_hide_excessive_alignment(self):
        source = "#define A APS5_VABI\n" + "\n".join(f"int A Decl{i}();" for i in range(8))
        source += "\nconstexpr int BufferSize = 16384;\nalignas(BufferSize) unsigned char storage[1];\n"
        self.assertEqual(self.findings(source), [("alignment", SOURCE, 12, "alignas(BufferSize)")])

    def test_only_added_code_is_reported(self):
        self.assertEqual(self.findings("alignas(256) char small[16];\n"
                                       "const char* text = \"alignas(65536)\";\n"
                                       "alignas(0x10000) char added[16];\n"
                                       "constexpr std::size_t BlockBytes = 65536;\n"
                                       "alignas(BlockBytes) std::array<std::uint8_t, BlockBytes> Block{};\n"),
                         [("alignment", SOURCE, 4, "alignas(0x10000)"), ("alignment", SOURCE, 6, "alignas(BlockBytes)")])

    def test_constants_are_resolved_in_their_scope(self):
        self.assertEqual(self.findings("namespace First {\n" + SMALL + LARGE + "}\nnamespace Second {\n" + LARGE + SMALL + "}\n"),
                         [("alignment", SOURCE, 9, "alignas(Alignment)"), ("alignment", SOURCE, 15, "alignas(Alignment)")])

    def test_reopened_namespace_keeps_its_constants(self):
        self.assertEqual(self.findings("namespace N {\n"
                                       "    constexpr std::size_t Alignment = 16384;\n"
                                       "}\n"
                                       "namespace N {\n"
                                       "    alignas(Alignment) static char data[4];\n"
                                       "}\n"),
                         [("alignment", SOURCE, 6, "alignas(Alignment)")])


if __name__ == "__main__":
    unittest.main()
