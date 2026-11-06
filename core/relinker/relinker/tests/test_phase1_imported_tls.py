"""Imported TLS relocation in the Windows executable: explicit rejection.

The current Windows relinker rejects R_X86_64_TPOFF64 against an undefined
STT_TLS symbol with "Imported TLS relocation ... is not supported for this
target" (WindowsRelocationBuilder.cpp:48-50). This test pins that explicit
rejection; it does not prove that imported TLS is resolved.

A future conformance test must bind the imported TLS symbol to the imported
module's TLS block and verify the resulting value; a non-empty PE TLS directory
alone is not proof of resolution. This is a CLI fixture against the real
relinker, not a mock.
"""

import subprocess
import sys
import tempfile
from pathlib import Path

from test_executable_tls_relocations import IMPORTED, TPOFF64, image

IMPORTED_TLS_OFFSET = 0x1000


def imported_tls_image():
    return image([(IMPORTED_TLS_OFFSET, TPOFF64, IMPORTED, 0)])


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-imported-tls-") as directory:
        work = Path(directory)
        source = work / "input.elf"
        output = work / "output.exe"
        source.write_bytes(imported_tls_image())
        result = subprocess.run(
            [str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
            capture_output=True, text=True, timeout=30)
        assert result.returncode == 2, (result.returncode, result.stdout, result.stderr)
        assert "Imported TLS relocation" in result.stderr, result.stderr
        assert "is not supported for this target" in result.stderr, result.stderr
        assert not output.exists(), "the relinker must not emit a PE for a rejected conversion"
        print("Imported TLS explicit rejection reproduced (not resolution)")


if __name__ == "__main__":
    main()
