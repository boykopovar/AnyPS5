from pathlib import Path
import subprocess
import sys
import tempfile

from test_macos_output import MachO
from test_tls_function_coverage import make_image, pe_bytes_at


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-string-segments-") as directory:
        work = Path(directory)
        for target in ("windows", "macos"):
            for segment in (0x64, 0x65):
                for opcode in (0x6c, 0x6d, 0xaa, 0xab, 0xae, 0xaf):
                    for prefixes in (b"", b"\x66", b"\xf3\x67", b"\xf2\x48"):
                        instruction = bytes([segment]) + prefixes + bytes([opcode])
                        body = instruction + b"\xc3"
                        name = target + "-" + instruction.hex()
                        source, output = work / (name + ".elf"), work / name
                        source.write_bytes(make_image("register", "unwind", body=body))
                        result = subprocess.run([str(relinker), "--" + target, "--skip-sce-module", str(source), str(output)],
                                                capture_output=True, text=True, timeout=20)
                        assert result.returncode == 0, (name, result.stdout, result.stderr)
                        data = output.read_bytes()
                        if target == "windows":
                            actual = pe_bytes_at(data, 0x11240, len(body))
                        else:
                            segment_info = next(s for s in MachO(data).segments if s["vmaddr"] <= 0x140011240 < s["vmaddr"] + s["filesize"])
                            offset = segment_info["fileoff"] + 0x140011240 - segment_info["vmaddr"]
                            actual = data[offset:offset + len(body)]
                        assert actual == body, (name, actual.hex(), body.hex())
    print("Ignored string segment prefixes preserved on Windows and macOS")


if __name__ == "__main__":
    main()
