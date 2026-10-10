from pathlib import Path
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture
from test_guest_needed_modules import executable_with_needed


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-module-path-") as directory:
        work = Path(directory)
        source = work / "input" / "test.elf"
        source.parent.mkdir()
        source.write_bytes(executable_with_needed())
        parent = work / "module root"
        modules = parent / "sce_module"
        modules.mkdir(parents=True)
        (modules / "needed.prx").write_bytes(guest_fixture(PLAIN_SITE))
        (work / "sce_module").write_text("must not inspect the working directory when a path is supplied")

        def run(label, options, cwd=work, error=None, artifact=False):
            output = work / label / "test.exe"
            output.parent.mkdir()
            result = subprocess.run([str(relinker), str(source), str(output), "--windows", *options], cwd=cwd, capture_output=True, text=True, timeout=30)
            if error:
                assert result.returncode != 0 and error in result.stderr, (label, result.stdout, result.stderr)
                assert not output.exists(), label
            else:
                assert result.returncode == 0, (label, result.stdout, result.stderr)
                assert output.is_file(), label
                assert (output.parent / "app0/sce_module/needed.prx.guest.prx").exists() == artifact, label

        run("absolute", ["--sce-module-path", str(parent)], artifact=True)
        run("relative", ["--sce-module-path", "module root"], artifact=True)
        inputModules = source.parent / "sce_module"
        inputModules.mkdir()
        (inputModules / "needed.prx").write_bytes(guest_fixture(PLAIN_SITE))
        run("default", [], artifact=True)
        (inputModules / "needed.prx").unlink()
        inputModules.rmdir()
        run("explicit-dot", ["--sce-module-path", "."], cwd=parent, artifact=True)
        run("skip", ["--skip-sce-module"])
        run("conflict", ["--sce-module-path", str(parent), "--skip-sce-module"], error="conflicts with --skip-sce-module")
        run("reverse-conflict", ["--skip-sce-module", "--sce-module-path", str(parent)], error="conflicts with --skip-sce-module")
        run("duplicate", ["--sce-module-path", ".", "--sce-module-path", "."], error="must be specified once")
        run("empty", ["--sce-module-path", ""], error="requires a nonempty path")
        run("missing-value", ["--sce-module-path"], error="requires a nonempty path")
        run("option-as-value", ["--sce-module-path", "--skip-sce-module"], error="requires a nonempty path")
        run("missing-parent", ["--sce-module-path", "absent"], error="parent directory does not exist")
        run("file-parent", ["--sce-module-path", str(source)], error="parent path is not a directory")
        run("wrong-level", ["--sce-module-path", str(modules)], error="--skip-sce-module")
        run("module-directory-is-file", ["--sce-module-path", "."], error="Guest module path is not a directory")
        (parent / "sce_modules").mkdir()
        run("ambiguous", ["--sce-module-path", str(parent)], error="Both sce_module and sce_modules")
        (parent / "sce_modules").rmdir()
        (modules / "needed.prx").unlink()
        nested = modules / "nested"
        nested.mkdir()
        (nested / "needed.prx").write_bytes(bytes.fromhex("4f153d1d") + bytes(128))
        run("no-recursion", ["--sce-module-path", str(parent)])
        (modules / "needed.prx").write_bytes(bytes.fromhex("4f153d1d") + bytes(128))
        run("self", ["--sce-module-path", str(parent)], error="SELF container")
        print("sce-module-path: 18 cases passed")


if __name__ == "__main__":
    main()
