import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import package_headers


def make_tree(root, headers):
    root.mkdir(parents=True, exist_ok=True)
    (root / "LICENSE").write_text("license\n", encoding="utf-8")
    for name, text in headers.items():
        path = root / "core/libs" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")


class PackageHeadersTest(unittest.TestCase):
    def test_collects_recursive_includes_and_ignores_commented_ones(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            make_tree(root, {
                "prx/libA/libA.hpp": '#include "../../common/shared.hpp"\n#include <vector>\n// #include "missing.hpp"\n',
                "common/shared.hpp": "#pragma once\n",
            })
            names = [path.name for path in package_headers.public_headers(root)]
            self.assertEqual(names, ["shared.hpp", "libA.hpp"])

    def test_rejects_missing_and_unsupported_includes(self):
        for text, message in (('#include "nope.hpp"\n', "Missing project"), ("#include <boost/any.hpp>\n", "Missing project"),
                              ("#include MACRO\n", "Non-literal"), ('#include "3rdparty/x.h"\n', "Forbidden")):
            with self.subTest(text=text), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                make_tree(root, {"prx/libA/libA.hpp": text})
                with self.assertRaisesRegex(RuntimeError, message):
                    package_headers.public_headers(root)

    def test_package_writes_both_archives_with_license(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "src"
            make_tree(root, {"prx/libA/libA.hpp": "#pragma once\n"})
            out = Path(tmp) / "out"
            package_headers.package(root, out, "v1.0")
            with zipfile.ZipFile(out / "prx-headers-v1.0.zip") as archive:
                self.assertEqual(sorted(archive.namelist()), ["LICENSE", "include/anyps5/prx/libA/libA.hpp"])
            self.assertTrue((out / "prx-headers-v1.0.tar.gz").is_file())
            self.assertEqual([p.name for p in out.iterdir() if p.name.startswith(".")], [])

    def test_package_rejects_invalid_version(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(ValueError):
                package_headers.package(Path(tmp), Path(tmp) / "out", "1.0/../x")


if __name__ == "__main__":
    unittest.main()
