from pathlib import Path
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture
from test_guest_tls import guest_with_tls
from test_tls_function_coverage import TLS_LOAD, make_image


TLS_INSTRUCTION = bytes.fromhex("64 8b 04 25 28 00 00 00")


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-tls-diagnostics-") as directory:
        work = Path(directory) / "input with spaces"
        work.mkdir()
        source = work / "eboot.elf"
        output = work / "output.exe"

        def check(input_path, offset, options):
            result = subprocess.run([str(relinker), "--windows", *options, str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 2, result
            assert "Unsupported Windows guest TLS instruction" in result.stderr, result.stderr
            assert f"(bytes: {TLS_INSTRUCTION.hex(' ')})" in result.stderr, result.stderr
            assert f"(offset {offset:#x})" in result.stderr, result.stderr
            assert f"Input: {input_path}\n" in result.stderr, result.stderr
            assert not output.exists(), output
            assert not list(work.rglob("*.guest.prx")), work

        image = make_image("register", "unwind")
        image[0x1240:0x1240 + len(TLS_LOAD)] = TLS_INSTRUCTION + b"\x90" * (len(TLS_LOAD) - len(TLS_INSTRUCTION))
        source.write_bytes(image)
        check(source, 0x1240, ["--skip-sce-module"])

        source.write_bytes(main_fixture())
        for name in ("sce_module", "sce_modules"):
            module_dir = work / name
            module_dir.mkdir()
            module = module_dir / "guest with spaces.prx"
            image = guest_with_tls(True)
            image[0x400:0x400 + len(TLS_INSTRUCTION) + 1] = TLS_INSTRUCTION + b"\xc3"
            module.write_bytes(image)
            check(module, 0x400, [])
            module.unlink()
            module_dir.rmdir()
    print("TLS diagnostics integration tests passed")


if __name__ == "__main__":
    main()
