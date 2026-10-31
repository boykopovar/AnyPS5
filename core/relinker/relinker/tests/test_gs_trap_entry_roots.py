"""Refuse Windows GS traps that would overwrite an executable entry point."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_windows_tls_protected_entries import make_image, pe_bytes_at


GS_LOAD_AND_RETURN = bytes.fromhex("65 48 8b 04 25 28 00 00 00 c3")
GS_VECTOR_AND_RETURN = bytes.fromhex("65 0f 10 04 25 28 00 00 00 c3")



def gs_image(root, instruction):
    image = make_image("relative" if root == "trap-tail" else root)
    image[0x210:0x21a] = instruction
    if root == "trap-tail":
        struct.pack_into("<q", image, 0x728, 0x215)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-gs-trap-entry-") as directory:
        work = Path(directory)

        def convert(name, instruction):
            source = work / (name + instruction.hex() + ".elf")
            output = source.with_suffix(".exe")
            source.write_bytes(gs_image(name, instruction))
            result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", "unused-filter=0",
                                     str(source), str(output)], capture_output=True, text=True, timeout=30)
            return result, output

        for instruction in (GS_LOAD_AND_RETURN, GS_VECTOR_AND_RETURN):
            for name in ("relative", "symbol", "lea", "initializer"):
                result, output = convert(name, instruction)
                assert result.returncode == 2, (name, result.returncode, result.stderr)
                assert "GS trap would overwrite an executable entry point" in result.stderr, (name, result.stderr)
                assert "(offset 0x214)" in result.stderr, (name, result.stderr)
                assert not output.exists(), name

            for name in ("safe", "access-start"):
                result, output = convert(name, instruction)
                assert result.returncode == 0 and output.exists(), (name, result.returncode, result.stderr)
                patched = pe_bytes_at(output.read_bytes(), 0x10210, len(instruction))
                if instruction == GS_LOAD_AND_RETURN:
                    assert patched[0] == 0xe9 and patched[5:] == instruction[5:], name
                else:
                    assert patched == b"\x0f\x0b" + b"\x90" * 7 + b"\xc3", name

        result, output = convert("trap-tail", GS_VECTOR_AND_RETURN)
        assert result.returncode == 2 and not output.exists(), result
        assert "GS trap would overwrite an executable entry point" in result.stderr, result.stderr
        assert "(offset 0x215)" in result.stderr, result.stderr

    print("Windows GS trap entry-point tests passed")


if __name__ == "__main__":
    main()
