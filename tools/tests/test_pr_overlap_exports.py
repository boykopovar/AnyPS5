import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap

TRAILING = "+extern \"C\" std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int) noexcept;   \n"
DIFF = """diff --git a/core/libs/prx/libkernel/File/src/Open.cpp b/core/libs/prx/libkernel/File/src/Open.cpp
@@ -96,0 +97,2 @@
+extern "C" int* APS5_VABI __error_nid_postfix();
""" + TRAILING + """@@ -135,0 +136,5 @@
+int APS5_VABI mlock_nid_postfix(const void* address, std::size_t length) {
+std::int64_t APS5_VABI sceKernelRead(int d, void* buf, std::size_t nbytes) noexcept {
+int APS5_VABI getpid_nid_postfix() { return 1; }
+int APS5_VABI sceNpUniversalDataSystemCreateEvent(
+void APS5_VABI Helper_nid_no_patch(int value) {
@@ -200 +205,0 @@
-int APS5_VABI removed_nid_postfix(int) {
diff --git a/core/libs/prx/libkernel/File/src/Stdio.cpp b/core/libs/prx/libkernel/File/src/Stdio.cpp
@@ -10,2 +10,2 @@
 int APS5_VABI context_nid_postfix(int) {
"""


class ExportTests(unittest.TestCase):
    def test_added_definitions_are_exports(self):
        self.assertEqual(pr_overlap.exports(DIFF), {"mlock_nid_postfix", "sceKernelRead", "getpid_nid_postfix", "sceNpUniversalDataSystemCreateEvent"})

    def test_declarations_are_not_exports(self):
        self.assertTrue(TRAILING.endswith(";   \n"))
        found = pr_overlap.exports(DIFF)
        self.assertNotIn("__error_nid_postfix", found)
        self.assertNotIn("recv_nid_postfix", found)

    def test_removed_context_and_helper_lines_are_not_exports(self):
        found = pr_overlap.exports(DIFF)
        self.assertNotIn("removed_nid_postfix", found)
        self.assertNotIn("context_nid_postfix", found)
        self.assertNotIn("Helper_nid_no_patch", found)


if __name__ == "__main__":
    unittest.main()
