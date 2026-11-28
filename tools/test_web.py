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
from test_game_patches import patch_site, patch_text
from test_guest_module_directories import module_with_symbol


def main(argv=None):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args(argv)
    build = Path(os.path.realpath(args.build))
    archive = build / "web/archive.mjs"
    if not archive.is_file():
        parser.error(f"archive module does not exist: {archive}")
    try:
        subprocess.run(["node", "--test", *(str(ROOT / "demo/web/tests" / name) for name in (
            "worker-manifest.test.mjs",
            "app.test.mjs", "batch.test.mjs", "failure-report.test.mjs", "i18n.test.mjs", "output-directory.test.mjs"
        ) + ("directory-writer.test.mjs",))], check=True)
        with tempfile.TemporaryDirectory(prefix="anyps5-web-") as directory:
            fixture = Path(directory)
            executable = bytes(main_fixture())
            (fixture / "main.elf").write_bytes(executable)
            address, expected = patch_site(executable)
            (fixture / "valid.toml").write_text(patch_text(address, expected, target="program.dat"), encoding="utf-8")
            (fixture / "mismatch.toml").write_text(patch_text(address, expected ^ 1, target="program.dat"), encoding="utf-8")
            module = bytes(module_with_symbol(True))
            (fixture / "module.elf").write_bytes(module)
            address, expected = patch_site(module)
            (fixture / "module.toml").write_text(
                patch_text(address, expected, target="sce_module/library.prx.esbak"), encoding="utf-8"
            )
            source = archive.read_text(encoding="utf-8-sig")
            match = re.search(r'from "(https://cdn.jsdelivr.net/[^"\n]+)"', source)
            if not match:
                parser.error(f"zip library URL not found in {archive}")
            url = match.group(1)
            library = fixture / "zip.mjs"
            with urllib.request.urlopen(url, timeout=30) as response:
                library.write_bytes(response.read())
            environment = dict(os.environ, ANYPS5_TEST_ZIP_URL=url, ANYPS5_TEST_ZIP_PATH=library.as_uri())
            web = str(build / "web")
            subprocess.run(["node", str(ROOT / "demo/web/tests/archive-root.test.mjs"), web], env=environment, check=True)
            subprocess.run(["node", str(ROOT / "demo/web/tests/archive-windows.test.mjs"), web], env=environment, check=True)
            subprocess.run(["node", str(ROOT / "demo/web/tests/archive-reserved.test.mjs"), web], env=environment, check=True)
            subprocess.run(["node", str(ROOT / "demo/web/tests/archive.test.mjs"), web, str(fixture)], env=environment, check=True)
    except (subprocess.CalledProcessError, urllib.error.URLError, OSError, UnicodeDecodeError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    sys.exit(main())
