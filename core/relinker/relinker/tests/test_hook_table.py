import json
import subprocess
import sys
import tempfile
from pathlib import Path

from test_call_registry import make_image


def run(relinker, *args):
    return subprocess.run([str(relinker), "--skip-sce-module", *args], capture_output=True, text=True, timeout=20)


def main():
    relinker = Path(sys.argv[1]).resolve()
    expected = [{"hook": 0, "nid": "symbol", "library": "", "targetOffsets": ["0x300"],
                 "callSites": ["0x200"], "callSitesResolved": True, "weak": False}]
    with tempfile.TemporaryDirectory(prefix="anyps5-hook-table-") as directory:
        work = Path(directory)
        source = work / "symbol.elf"
        source.write_bytes(make_image(b"\0lib.so\0symbol\0"))
        for options in ([], ["--windows"]):
            suffix = "-windows.exe" if options else "-linux.out"
            output = work / ("hooks" + suffix)
            result = run(relinker, "--registry", *options, str(source), str(output))
            if result.returncode != 0 or not output.exists():
                raise AssertionError((options, result.returncode, result.stdout, result.stderr))
            hooks = json.loads(output.with_suffix(".hooks.json").read_text(encoding="utf-8"))
            if hooks != expected:
                raise AssertionError((options, hooks))
            json.loads(output.with_suffix(".registry.json").read_text(encoding="utf-8"))
            json.loads(output.with_suffix(".needed.json").read_text(encoding="utf-8"))

        plain = work / "plain-linux.out"
        result = run(relinker, str(source), str(plain))
        if result.returncode != 0 or not plain.exists():
            raise AssertionError((result.returncode, result.stdout, result.stderr))
        if plain.with_suffix(".hooks.json").exists():
            raise AssertionError("hook table written without --registry")

        dry = work / "dry-linux.out"
        result = run(relinker, "--registry", "--dry-run", str(source), str(dry))
        if result.returncode != 0 or dry.exists() or dry.with_suffix(".hooks.json").exists():
            raise AssertionError((result.returncode, result.stdout, result.stderr))
        if not any(line.startswith("Would write: ") and line.endswith("dry-linux.hooks.json") for line in result.stdout.splitlines()):
            raise AssertionError(("dry-run plan", result.stdout))
    print("Hook table JSON tests passed")


if __name__ == "__main__":
    main()
