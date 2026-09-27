import importlib.util
import pathlib
import unittest


SCRIPT = pathlib.Path(__file__).with_name("audit-elf-imports.py")
SPEC = importlib.util.spec_from_file_location("audit_elf_imports", SCRIPT)
AUDIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AUDIT)


class ImportAuditTests(unittest.TestCase):
    def test_known_nid(self):
        self.assertEqual(AUDIT.nid("sceUserServiceGetForegroundUser"), "eNb53LQJmIM")

    def test_export_table_pattern(self):
        text = "[Ordinal/Name Pointer] Table\n\t[ 2] eNb53LQJmIM\n\t[ 3] plain_name\n"
        self.assertEqual(AUDIT.EXPORT.findall(text), ["eNb53LQJmIM", "plain_name"])


if __name__ == "__main__":
    unittest.main()
