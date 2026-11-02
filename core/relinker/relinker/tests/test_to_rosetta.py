"""Check that --to-rosetta lowers what Rosetta lacks, and that --to-intel and a plain relink leave it alone."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_linux_load_alignment import fixture

RDSEED = bytes.fromhex("0fc7f8")
RDRAND = bytes.fromhex("0fc7f0")
CLWB = bytes.fromhex("660fae37")
NOP4 = bytes.fromhex("0f1f4000")
XSAVEOPT = bytes.fromhex("480fae37")
XSAVE = bytes.fromhex("480fae27")
SITES = RDSEED + CLWB + XSAVEOPT + b"\xc3"
LOWERED = RDRAND + NOP4 + XSAVE + b"\xc3"


def executable(code=SITES):
    image = fixture()
    struct.pack_into("<Q", image, 24, 0x10)
    image[0x4010:0x4010 + len(code)] = code
    return image


def relink(relinker, directory, name, *options, code=SITES):
    source = Path(directory) / f"{name}.elf"
    output = Path(directory) / f"{name}.out"
    source.write_bytes(bytes(executable(code)))
    result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)], capture_output=True, text=True, timeout=30)
    return result, output


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-rosetta-") as directory:
        for options in ((), ("--to-intel",)):
            result, output = relink(relinker, directory, "keep" + "".join(options), *options)
            assert result.returncode == 0, (options, result.stdout, result.stderr)
            data = output.read_bytes()
            assert SITES in data and LOWERED not in data, f"{options or 'a plain relink'} changed RDSEED, CLWB or XSAVEOPT"
        for options in (("--to-rosetta",), ("--to-rosetta", "--to-intel")):
            result, output = relink(relinker, directory, "lower" + "".join(options), *options)
            assert result.returncode == 0, (options, result.stdout, result.stderr)
            data = output.read_bytes()
            assert LOWERED in data and SITES not in data, f"{options} did not lower RDSEED, CLWB and XSAVEOPT"
        for options, lowered in ((("--macos",), False), (("--macos", "--to-intel"), False), (("--macos", "--to-rosetta"), True)):
            result, output = relink(relinker, directory, "macos" + "".join(options), *options)
            assert result.returncode == 0, (options, result.stdout, result.stderr)
            data = output.read_bytes()
            assert data[:4] == bytes.fromhex("cffaedfe"), f"{options} did not write a Mach-O"
            assert (LOWERED in data and SITES not in data) if lowered else (SITES in data and LOWERED not in data), f"{options} {'did not lower' if lowered else 'changed'} RDSEED, CLWB and XSAVEOPT"
        result, _ = relink(relinker, directory, "windows", "--to-rosetta", "--windows")
        assert result.returncode != 0 and "--to-rosetta conflicts with --windows" in result.stdout + result.stderr, "--to-rosetta together with --windows was accepted"
        andn = bytes.fromhex("c4e2f0f2c6c3")
        for target in ((), ("--macos",)):
            for conversion in ((), ("--to-intel",), ("--to-rosetta",)):
                options = target + conversion
                result, output = relink(relinker, directory, "andn" + "".join(options), *options, code=andn)
                assert result.returncode == 0, (options, result.stdout, result.stderr)
                lowered = conversion == ("--to-rosetta",)
                assert (andn not in output.read_bytes()) == lowered, f"{options} {'did not lower' if lowered else 'changed'} ANDN"
                if lowered:
                    assert "ANDN" in result.stdout, result.stdout
        masked = bytes.fromhex("c4e2752e07c3")
        for target in ((), ("--macos",)):
            for conversion in ((), ("--to-intel",), ("--to-rosetta",)):
                options = target + conversion
                result, output = relink(relinker, directory, "masked" + "".join(options), *options, code=masked)
                assert result.returncode == 0, (options, result.stdout, result.stderr)
                lowered = conversion == ("--to-rosetta",)
                assert (masked not in output.read_bytes()) == lowered, f"{options} {'did not lower' if lowered else 'changed'} VMASKMOVPS"
                if lowered:
                    assert "VMASKMOVPS" in result.stdout, result.stdout
    print("--to-rosetta integration tests passed")


if __name__ == "__main__":
    main()
