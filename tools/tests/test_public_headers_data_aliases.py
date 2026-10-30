import importlib.util
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("public_headers_audit", ROOT / "core/libs/tests/test_public_headers.py")
AUDIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AUDIT)


class PublicHeaderDataAliases(unittest.TestCase):
    def run_audit(self, header, source):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            libs = root / "core/libs"
            lib = libs / "prx/libExample"
            lib.mkdir(parents=True)
            (lib / "libExample.hpp").write_text(header, encoding="utf-8")
            (lib / "Export.cpp").write_text(source, encoding="utf-8")
            AUDIT.source_text.cache_clear()
            AUDIT.included_headers.cache_clear()
            with patch.object(AUDIT, "ROOT", root), patch.object(AUDIT, "LIBS", libs):
                return AUDIT.audit()

    def test_data_alias_requires_one_reachable_data_declaration(self):
        source = 'APS5_EXPORT("AAAAAAAAAAA", ExampleData);\n'
        self.run_audit('extern const char ExampleData[];\n', source)
        with self.assertRaisesRegex(RuntimeError, "ExampleData: expected exactly one header declaration"):
            self.run_audit('', source)
        with self.assertRaisesRegex(RuntimeError, "ExampleData: expected exactly one header declaration"):
            self.run_audit('extern const char ExampleData[];\nextern const char ExampleData[];\n', source)

    def test_function_definition_cannot_be_declared_as_data(self):
        source = 'int APS5_VABI ExampleFunction() { return 1; }\n'
        self.run_audit('int APS5_VABI ExampleFunction();\n', source)
        with self.assertRaisesRegex(RuntimeError, "ExampleFunction: function definition requires a function declaration"):
            self.run_audit('extern const char ExampleFunction[];\n', source)


if __name__ == "__main__":
    unittest.main()
