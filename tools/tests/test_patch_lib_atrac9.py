import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
SCRIPT = REPOSITORY / "core/libs/prx/libSceAjm.native/PatchLibAtrac9.cmake"

UNPACK_GUARD = "block->GradientEndUnit < 0 || block->GradientEndUnit >= 48"
BIT_READER_PEEK = "\tconst int byteIndex = br->Position / 8;\n\tconst int bitIndex = br->Position % 8;\n\tconst int numBytes = (bitIndex + bits - 1) / 8 + 1;\n\tconst unsigned char* buffer = br->Buffer;\n\n\tint value;\n\tswitch (numBytes) {"
DECINIT_GUARD = "header != 0xFE || validationBit != 0"
UTILITY_REVERSE = "\treturn value >> (32 - bitCount);"
UTILITY_SIGN_EXTEND = "\treturn (value << shift) >> shift;"


def write_sources(root, extra_unpack=""):
    root.mkdir(parents=True, exist_ok=True)
    (root / "unpack.c").write_text("int unpack(void) { if (" + UNPACK_GUARD + ") return 1; return 0; }\n" + extra_unpack)
    (root / "bit_reader.c").write_text("int peek(void) {\n" + BIT_READER_PEEK + "\n\treturn 0; }\n")
    (root / "decinit.c").write_text("int decinit(void) { return (" + DECINIT_GUARD + "); }\n")
    (root / "utility.c").write_text("int reverse(void) {\n" + UTILITY_REVERSE + "\n}\nint sign_extend(void) {\n" + UTILITY_SIGN_EXTEND + "\n}\n")


class PatchLibAtrac9Tests(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory(prefix="anyps5-atrac9-patch-")
        self.work = Path(self._directory.name)
        self.sources = self.work / "src"
        self.output = self.work / "generated"
        write_sources(self.sources)
        self.wrapper = self.work / "run.cmake"

    def tearDown(self):
        self._directory.cleanup()

    def run_patch(self, sources=None):
        self.wrapper.write_text(
            'set(LIBATRAC9_DIR "${CMAKE_CURRENT_LIST_DIR}/' + (sources or self.sources).relative_to(self.work).as_posix() + '")\n'
            'set(LIBATRAC9_PATCHED_DIR "${CMAKE_CURRENT_LIST_DIR}/generated")\n'
            'include("' + SCRIPT.as_posix() + '")\n'
        )
        return subprocess.run([sys.argv[1], "-P", str(self.wrapper)], capture_output=True, text=True, timeout=60, cwd=self.work)

    def set_sentinel(self):
        for name in ("unpack.c", "bit_reader.c", "decinit.c", "utility.c"):
            stamp = 946684800
            path = self.output / name
            os.utime(path, (stamp, stamp))

    def mtime(self, name):
        return int((self.output / name).stat().st_mtime)

    def test_patches_and_rewrites_only_changed_outputs(self):
        result = self.run_patch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("block->GradientEndUnit > 31", (self.output / "unpack.c").read_text())
        self.assertIn(">> 3", (self.output / "bit_reader.c").read_text())
        self.assertIn("config->ChannelConfigIndex > 5", (self.output / "decinit.c").read_text())
        self.assertIn(": 0u;", (self.output / "utility.c").read_text())
        self.set_sentinel()
        result = self.run_patch()
        self.assertEqual(result.returncode, 0, result.stderr)
        for name in ("unpack.c", "bit_reader.c", "decinit.c", "utility.c"):
            self.assertEqual(self.mtime(name), 946684800, name + " was rewritten without a change")
        write_sources(self.sources, extra_unpack="int changed(void) { return 2; }\n")
        self.set_sentinel()
        result = self.run_patch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("changed", (self.output / "unpack.c").read_text())
        self.assertNotEqual(self.mtime("unpack.c"), 946684800, "a changed input did not rewrite its output")
        for name in ("bit_reader.c", "decinit.c", "utility.c"):
            self.assertEqual(self.mtime(name), 946684800, name + " was rewritten without a change")

    def test_missing_marker_fails(self):
        (self.sources / "unpack.c").write_text("int unpack(void) { return 0; }\n")
        result = self.run_patch()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("gradient guard has changed", result.stderr)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
