import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile

from test_dynamic_segment import fixture


def main():
    relinker = Path(sys.argv[1]).resolve()
    mode = sys.argv[2]
    flags = [] if mode == "linux" else ["--" + mode]
    with tempfile.TemporaryDirectory(prefix="anyps5-output-fifo-") as directory:
        work = Path(directory)
        source = work / "input.elf"
        image = fixture()
        source.write_bytes(image)

        def convert(output, *options):
            return subprocess.run(
                [
                    str(relinker),
                    "--skip-sce-module",
                    *flags,
                    *options,
                    str(source),
                    str(output),
                ],
                capture_output=True,
                text=True,
                timeout=5,
            )

        control_dir = work / "control"
        control_dir.mkdir()
        control = control_dir / "output"
        result = convert(control)
        assert result.returncode == 0, result.stderr
        expected = control.read_bytes()
        for name in ("regular", "symlink"):
            case = work / name
            case.mkdir()
            output = case / "output"
            target = case / "output-target" if name == "symlink" else output
            target.write_bytes(b"previous")
            if name == "symlink":
                output.symlink_to(target.name)
            result = convert(output)
            assert result.returncode == 0, result.stderr
            assert target.read_bytes() == expected, name
            assert name != "symlink" or output.is_symlink(), name

        for name in ("direct", "symlink", "registry", "needed"):
            case = work / ("fifo-" + name)
            case.mkdir()
            output = case / "output"
            pipe = output if name == "direct" else case / "pipe"
            os.mkfifo(pipe)
            options = []
            if name == "symlink":
                output.symlink_to(pipe.name)
            elif name in ("registry", "needed"):
                options = ["--registry"]
                output.write_bytes(b"previous executable")
                for suffix in ("registry", "needed"):
                    path = case / ("output." + suffix + ".json")
                    if suffix == name:
                        path.symlink_to(pipe.name)
                    else:
                        path.write_bytes(b"previous metadata")
            result = convert(output, *options)
            assert result.returncode == 2 and "Output is a FIFO:" in result.stderr, (
                name,
                result,
            )
            assert stat.S_ISFIFO(pipe.stat().st_mode), name
            if name == "symlink":
                assert output.is_symlink(), name
            elif name in ("registry", "needed"):
                assert output.read_bytes() == b"previous executable", name
                other = "needed" if name == "registry" else "registry"
                assert (
                    case / ("output." + other + ".json")
                ).read_bytes() == b"previous metadata", name
                assert (case / ("output." + name + ".json")).is_symlink(), name
            assert source.read_bytes() == image, name
    print(mode + " FIFO output rejection and regular-file controls passed")


if __name__ == "__main__":
    main()
