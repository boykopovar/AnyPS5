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
SITES = RDSEED + CLWB + b"\xc3"
LOWERED = RDRAND + NOP4 + b"\xc3"


def executable():
    image = fixture()
    struct.pack_into("<Q", image, 24, 0x10)
    image[0x4010:0x4010 + len(SITES)] = SITES
    return image


def relink(relinker, directory, name, *options):
    source = Path(directory) / f"{name}.elf"
    output = Path(directory) / f"{name}.out"
    source.write_bytes(bytes(executable()))
    result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)], capture_output=True, text=True, timeout=30)
    return result, output


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-rosetta-") as directory:
        for options in ((), ("--to-intel",)):
            result, output = relink(relinker, directory, "keep" + "".join(options), *options)
            assert result.returncode == 0, (options, result.stdout, result.stderr)
            data = output.read_bytes()
            assert SITES in data and LOWERED not in data, f"{options or 'a plain relink'} changed RDSEED or CLWB"
        for options in (("--to-rosetta",), ("--to-rosetta", "--to-intel")):
            result, output = relink(relinker, directory, "lower" + "".join(options), *options)
            assert result.returncode == 0, (options, result.stdout, result.stderr)
            data = output.read_bytes()
            assert LOWERED in data and SITES not in data, f"{options} did not lower RDSEED and CLWB"
        for options, lowered in ((("--macos",), False), (("--macos", "--to-intel"), False), (("--macos", "--to-rosetta"), True)):
            result, output = relink(relinker, directory, "macos" + "".join(options), *options)
            assert result.returncode == 0, (options, result.stdout, result.stderr)
            data = output.read_bytes()
            assert data[:4] == bytes.fromhex("cffaedfe"), f"{options} did not write a Mach-O"
            assert (LOWERED in data and SITES not in data) if lowered else (SITES in data and LOWERED not in data), f"{options} {'did not lower' if lowered else 'changed'} RDSEED and CLWB"
        result, _ = relink(relinker, directory, "windows", "--to-rosetta", "--windows")
        assert result.returncode != 0 and "--to-rosetta conflicts with --windows" in result.stdout + result.stderr, "--to-rosetta together with --windows was accepted"
    print("--to-rosetta integration tests passed")


if __name__ == "__main__":
    main()
