"""Refuse Windows GS traps that would overwrite an executable entry point."""

from pathlib import Path
import subprocess
import sys
import tempfile

from test_windows_tls_protected_entries import make_image, pe_bytes_at


GS_LOAD_AND_RETURN = bytes.fromhex("65 48 8b 04 25 28 00 00 00 c3")
TRAPPED = bytes.fromhex("0f 0b 90 90 90 90 90 90 90 c3")


def gs_image(root):
    image = make_image(root)
    image[0x210:0x21a] = GS_LOAD_AND_RETURN
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-gs-trap-entry-") as directory:
        work = Path(directory)

        def convert(name):
            source = work / (name + ".elf")
            output = work / (name + ".exe")
            source.write_bytes(gs_image(name))
            result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", "unused-filter=0",
                                     str(source), str(output)], capture_output=True, text=True, timeout=30)
            return result, output

        for name in ("relative", "symbol", "lea", "initializer"):
            result, output = convert(name)
            assert result.returncode == 2, (name, result.returncode, result.stderr)
            assert "GS trap would overwrite an executable entry point" in result.stderr, (name, result.stderr)
            assert "(offset 0x214)" in result.stderr, (name, result.stderr)
            assert not output.exists(), name

        for name in ("safe", "access-start"):
            result, output = convert(name)
            assert result.returncode == 0 and output.exists(), (name, result.returncode, result.stderr)
            assert pe_bytes_at(output.read_bytes(), 0x10210, len(TRAPPED)) == TRAPPED, name

    print("Windows GS trap entry-point tests passed")


if __name__ == "__main__":
    main()
