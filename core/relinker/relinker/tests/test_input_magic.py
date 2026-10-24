"""Reject inputs that are not plain ELF files with a message that names what was read."""

from pathlib import Path
import platform
import subprocess
import sys
import tempfile


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-magic-") as directory:
        work = Path(directory)

        def convert(name, content, error):
            source = work / name
            source.write_bytes(content)
            output = work / (name + ".out")
            result = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 2 and error in result.stderr and not output.exists(), (name, result)

        convert("eboot.self", b"\x4f\x15\x3d\x1d" + bytes(0x1000), "The input is a SELF container, not an ELF")
        convert("eboot.self.kernel", b"\x54\x14\xf5\xee" + bytes(0x1000), "The input is a SELF container, not an ELF")
        convert("eboot.pkg", b"\x7fCNT" + bytes(0x1000), "Invalid ELF magic number: 7f 43 4e 54")
        convert("eboot.short", b"\x7fEL", "File too small for ELF header")
        unsupported_version = bytearray(64)
        unsupported_version[:4] = b"\x7fELF"
        unsupported_version[4:7] = b"\x02\x01\x01"
        unsupported_version[0x12:0x14] = b"\x3e\x00"
        convert("eboot.unsupported-version", unsupported_version, "Unsupported ELF object version: expected 1")
        if platform.system() == "Linux":
            import resource

            source = work / "large-unsupported.pkg"
            output = work / "large-unsupported.out"
            with source.open("wb") as stream:
                stream.write(b"\x7fFIH")
                stream.truncate(8 * 1024**3)

            def limit_address_space():
                resource.setrlimit(resource.RLIMIT_AS, (512 * 1024**2, 512 * 1024**2))
                resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

            result = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(output)],
                                    capture_output=True, text=True, timeout=20, preexec_fn=limit_address_space)
            assert result.returncode == 2 and "Invalid ELF magic number: 7f 46 49 48" in result.stderr and not output.exists(), result
    print("Input magic diagnostics tests passed")


if __name__ == "__main__":
    main()
