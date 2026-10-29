import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "core/relinker/relinker/tests"))
from test_guest_intel_trampolines import main_fixture
from test_guest_module_directories import module_with_symbol


def main(argv=None):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args(argv)
    archive = args.build / "web/archive.mjs"
    if not archive.is_file():
        parser.error(f"archive module does not exist: {archive}")
    try:
        subprocess.run(["node", "--test", *(str(ROOT / "demo/web/tests" / name) for name in (
            "app.test.mjs", "batch.test.mjs", "failure-report.test.mjs", "output-directory.test.mjs"
        ) + ("directory-writer.test.mjs",))], check=True)
        with tempfile.TemporaryDirectory(prefix="anyps5-web-") as directory:
            fixture = Path(directory)
            (fixture / "main.elf").write_bytes(main_fixture())
            (fixture / "module.elf").write_bytes(module_with_symbol(True))
            source = archive.read_text(encoding="utf-8-sig")
            match = re.search(r'from "(https://cdn.jsdelivr.net/[^"\n]+)"', source)
            if not match:
                parser.error(f"zip library URL not found in {archive}")
            url = match.group(1)
            library = fixture / "zip.mjs"
            with urllib.request.urlopen(url, timeout=30) as response:
                library.write_bytes(response.read())
            environment = dict(os.environ, ANYPS5_TEST_ZIP_URL=url, ANYPS5_TEST_ZIP_PATH=library.as_uri())
            subprocess.run(["node", str(ROOT / "demo/web/tests/archive-root.test.mjs"), str(args.build.resolve() / "web")], env=environment, check=True)
            subprocess.run(["node", str(ROOT / "demo/web/tests/archive-windows.test.mjs"), str(args.build.resolve() / "web")], env=environment, check=True)
            subprocess.run(["node", str(ROOT / "demo/web/tests/archive-reserved.test.mjs"), str(args.build.resolve() / "web")], env=environment, check=True)
            subprocess.run(["node", str(ROOT / "demo/web/tests/archive.test.mjs"), str(args.build.resolve() / "web"), str(fixture)], env=environment, check=True)
    except (subprocess.CalledProcessError, urllib.error.URLError, OSError, UnicodeDecodeError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    sys.exit(main())
