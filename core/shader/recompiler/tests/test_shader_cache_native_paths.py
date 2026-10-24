from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(sys.argv.pop(1)).resolve()
CMAKE = sys.argv.pop(1)


class ShaderCachePathTests(unittest.TestCase):
    def fixture(self, root):
        source = root / 'recompiler' / 'input.cpp'
        source.parent.mkdir(parents=True)
        source.write_text('const char* mode = "APS5_TEST_PATH";\n', encoding='utf-8')
        listing = root / 'sources.txt'
        listing.write_text(f'sources {source.as_posix()}\n', encoding='utf-8')
        output = root / 'version.hpp'
        return source, listing, output

    def run_script(self, root, listing, output):
        return subprocess.run([CMAKE, f'-DROOT={root.as_posix()}',
                               f'-DRECOMPILER_DIR={(root / "recompiler").as_posix()}',
                               f'-DLIST_FILE={listing.as_posix()}', f'-DOUTPUT={output.as_posix()}',
                               '-P', str(SCRIPT)], capture_output=True, encoding='utf-8', errors='replace', timeout=10)

    def check_source_change(self, name):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / name
            source, listing, output = self.fixture(root)
            result = self.run_script(root, listing, output)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            previous = output.read_bytes()
            self.assertIn(b'APS5_TEST_PATH', previous)
            source.write_text('const char* mode = "APS5_TEST_CHANGED";\n', encoding='utf-8')
            result = self.run_script(root, listing, output)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertNotEqual(previous, output.read_bytes())
            self.assertIn(b'APS5_TEST_CHANGED', output.read_bytes())
            unchanged = output.stat().st_mtime_ns
            result = self.run_script(root, listing, output)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(output.stat().st_mtime_ns, unchanged)

    def test_ascii(self):
        self.check_source_change('plain path')

    def test_cyrillic(self):
        self.check_source_change('\u0441\u0431\u043e\u0440\u043a\u0430 \u0438\u0433\u0440\u044b')

    def test_cjk(self):
        self.check_source_change('\u6e2c\u8a66 path')

    def test_unicode_guard_rejects_unhashed_include(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / '\u0441\u0431\u043e\u0440\u043a\u0430'
            source, listing, output = self.fixture(root)
            (root / 'shared.hpp').write_text('int example();\n', encoding='utf-8')
            source.write_text('#include "../shared.hpp"\n', encoding='utf-8')
            result = self.run_script(root, listing, output)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('does not hash', result.stdout + result.stderr)
            self.assertFalse(output.exists())

    def test_unicode_header_rejects_unhashed_include(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'plain path'
            source, listing, output = self.fixture(root)
            name = '\u0437\u0430\u0433\u043e\u043b\u043e\u0432\u043e\u043a.hpp'
            (root / name).write_text('int example();\n', encoding='utf-8')
            source.write_text(f'#include "../{name}"\n', encoding='utf-8')
            result = self.run_script(root, listing, output)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('does not hash', result.stdout + result.stderr)
            self.assertFalse(output.exists())

    @unittest.skipUnless(sys.platform == 'win32', 'Windows path separator semantics require Windows host')
    def test_windows_path_separators(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'windows separators'
            source, listing, output = self.fixture(root)
            windows_path = str(source).replace('/', '\\')
            listing.write_text(f'sources {windows_path}\n', encoding='utf-8')
            result = self.run_script(root, listing, output)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            windows_version = output.read_bytes()
            listing.write_text(f'sources {source.as_posix()}\n', encoding='utf-8')
            result = self.run_script(root, listing, output)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(windows_version, output.read_bytes())

    @unittest.skipUnless(sys.platform == 'win32', 'Windows path separator semantics require Windows host')
    def test_windows_path_separators_reject_unhashed_include(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'windows separators'
            source, listing, output = self.fixture(root)
            windows_path = str(source).replace('/', '\\')
            listing.write_text(f'sources {windows_path}\n', encoding='utf-8')
            (root / 'shared.hpp').write_text('int example();\n', encoding='utf-8')
            source.write_text('#include "../shared.hpp"\n', encoding='utf-8')
            result = self.run_script(root, listing, output)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('does not hash', result.stdout + result.stderr)
            self.assertFalse(output.exists())


unittest.main()
