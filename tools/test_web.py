import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "core/relinker/relinker/tests"))
from test_guest_intel_trampolines import main_fixture
from test_guest_module_directories import module_with_symbol


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args()
    subprocess.run(["node", "--test", str(ROOT / "demo/web/tests/output-directory.test.mjs")], check=True)
    with tempfile.TemporaryDirectory(prefix="anyps5-web-") as directory:
        fixture = Path(directory)
        (fixture / "main.elf").write_bytes(main_fixture())
        (fixture / "module.elf").write_bytes(module_with_symbol(True))
        source = (args.build / "web/archive.mjs").read_text(encoding="utf-8")
        url = re.search(r'from "(https://cdn.jsdelivr.net/[^"\n]+)"', source).group(1)
        library = fixture / "zip.mjs"
        with urllib.request.urlopen(url, timeout=30) as response:
            library.write_bytes(response.read())
        environment = dict(os.environ, ANYPS5_TEST_ZIP_URL=url, ANYPS5_TEST_ZIP_PATH=library.as_uri())
        subprocess.run(["node", str(ROOT / "demo/web/tests/archive-root.test.mjs"), str(args.build.resolve() / "web")], env=environment, check=True)
        subprocess.run(["node", str(ROOT / "demo/web/tests/archive-windows.test.mjs"), str(args.build.resolve() / "web")], env=environment, check=True)
        subprocess.run(["node", str(ROOT / "demo/web/tests/archive.test.mjs"), str(args.build.resolve() / "web"), str(fixture)], env=environment, check=True)


if __name__ == "__main__":
    main()
