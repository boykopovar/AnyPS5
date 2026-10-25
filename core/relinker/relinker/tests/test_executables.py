from pathlib import Path
import os
import stat
import subprocess
import sys
import tempfile

from test_guest_host_libc import importing
from test_guest_intel_trampolines import main_fixture
from test_guest_module_dir import pe_export_names
from test_guest_module_directories import module_with_symbol, needed_libraries
from test_guest_needed_modules import executable_with_needed


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-executables-") as directory:
        work = Path(directory)

        def convert(case, windows, options, picker=None, eboot=None):
            (case / "sce_module").mkdir(parents=True, exist_ok=True)
            (case / "sce_module" / "bundled.prx").write_bytes(module_with_symbol(True))
            (case / "picker").mkdir(exist_ok=True)
            (case / "picker" / "picker.bin").write_bytes(main_fixture() if picker is None else picker)
            source = case / "eboot.bin"
            source.write_bytes(main_fixture() if eboot is None else eboot)
            output = case / "out" / ("app.exe" if windows else "app.elf")
            output.parent.mkdir(exist_ok=True)
            result = subprocess.run([str(relinker), *(["--windows"] if windows else []), "--registry", *options, str(source), str(output)],
                                    capture_output=True, text=True, timeout=60)
            return result, output

        for windows in (False, True):
            suffix = ".exe" if windows else ".elf"
            case = work / f"{windows}-picker"
            result, output = convert(case, windows, ["--executable", "picker/picker.bin"])
            assert result.returncode == 0, (result.stdout, result.stderr)
            secondary = output.parent / ("picker.picker.bin" + suffix)
            assert secondary.read_bytes().startswith(b"MZ" if windows else b"\x7fELF"), secondary
            assert "Executable: picker/picker.bin\n" in result.stdout, result.stdout
            assert f"Executable file: {secondary}\n" in result.stdout, result.stdout
            assert f"  |-- app{suffix}\n  |-- picker.picker.bin{suffix}\n  |-- libs/\n" in result.stdout, result.stdout
            assert (output.parent / "picker.picker.bin.registry.json").is_file()
            assert sorted(path.name for path in output.parent.rglob("*.guest.prx")) == ["bundled.prx.guest.prx"]
            if not windows:
                assert "$ORIGIN/app0/sce_module/bundled.prx.guest.prx" in needed_libraries(secondary.read_bytes())
                if os.name != "nt":
                    for executable in (output, secondary):
                        assert stat.S_IMODE(executable.stat().st_mode) & 0o111 == 0o111, (executable, oct(executable.stat().st_mode))
            if windows and os.name == "nt":
                for executable in (output, secondary):
                    run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
                    assert run.returncode == 42, (executable, run.returncode, run.stdout, run.stderr)

            for options, message in ((["--executable", "../eboot.bin"], "inside the guest module parent directory"),
                                     (["--executable", "picker"], "is not a file"),
                                     (["--executable", "picker/missing.bin"], "is not a file"),
                                     (["--executable", "eboot.bin"], "converted twice"),
                                     (["--executable", "picker/picker.bin", "--executable", "picker/./picker.bin"], "converted twice"),
                                     (["--module-dir", "picker", "--executable", "picker/picker.bin"], "in a guest module directory"),
                                     (["--module-dir", "picker/", "--executable", "picker/picker.bin"], "in a guest module directory")):
                case = work / f"{windows}-rejected-{len(message)}-{len(options)}"
                result, output = convert(case, windows, options)
                assert result.returncode == 2 and message in result.stderr, (options, result.stderr)
                assert not output.exists(), options

            for linked, options in (("eboot.exe", ["--executable", "picker/eboot.exe"]),
                                    ("picker.exe", ["--executable", "picker/picker.bin", "--executable", "picker/picker.exe"])):
                case = work / f"{windows}-hardlink-{linked}"
                (case / "picker").mkdir(parents=True)
                target = case / ("eboot.bin" if linked == "eboot.exe" else "picker/picker.bin")
                target.write_bytes(main_fixture())
                os.link(target, case / "picker" / linked)
                result, output = convert(case, windows, options)
                assert result.returncode == 2 and "converted twice" in result.stderr, (linked, result.stdout, result.stderr)
                assert not output.exists(), linked

            for index, (directory, executable) in enumerate((("Picker", "picker/picker.bin"), ("picker", "PICKER/picker.bin"), ("linked", "picker/picker.bin"))):
                case = work / f"{windows}-identity-{index}"
                case.mkdir()
                insensitive = (work / f"{windows}-IDENTITY-{index}").exists()
                if (directory == "linked" and os.name == "nt") or (executable != "picker/picker.bin" and not insensitive):
                    continue
                if directory == "linked":
                    (case / "linked").symlink_to("picker", target_is_directory=True)
                elif not insensitive:
                    (case / directory).mkdir()
                result, output = convert(case, windows, ["--module-dir", directory, "--executable", executable])
                if insensitive or directory == "linked":
                    assert result.returncode == 2 and "in a guest module directory" in result.stderr, (directory, executable, result.stderr)
                    assert not output.exists(), (directory, executable)
                else:
                    assert result.returncode == 0, (directory, executable, result.stdout, result.stderr)

            case = work / f"{windows}-needed"
            (case / "Media" / "Modules").mkdir(parents=True)
            (case / "Media" / "Modules" / "needed.prx").write_bytes(module_with_symbol(True))
            result, output = convert(case, windows, ["--module-dir", "Media/Modules", "--executable", "picker/picker.bin"], executable_with_needed())
            if windows:
                assert result.returncode == 0, (result.stdout, result.stderr)
                assert (output.parent / "picker.picker.bin.exe").exists()
                assert "__aps5_guest_initialize" in pe_export_names((output.parent / "app0" / "Media" / "Modules" / "needed.prx.guest.prx").read_bytes())
            else:
                assert result.returncode == 0, (result.stdout, result.stderr)
                secondary = output.parent / "picker.picker.bin.elf"
                assert "$ORIGIN/app0/Media/Modules/needed.prx.guest.prx" in needed_libraries(secondary.read_bytes())
                assert "$ORIGIN/app0/Media/Modules/needed.prx.guest.prx" not in needed_libraries(output.read_bytes())

        for label, windows, target in (("linux", False, []), ("macos", False, ["--macos"]), ("windows", True, [])):
            for name, picker in (("subset", importing("libScePad.prx")), ("order", importing("libSceVideoOut.prx", "libScePad.prx"))):
                case = work / f"{label}-host-{name}"
                result, output = convert(case, windows, [*target, "--executable", "picker/picker.bin"], picker, importing("libScePad.prx", "libSceVideoOut.prx"))
                assert result.returncode == 0, (label, name, result.stdout, result.stderr)
                modules = sorted(output.parent.rglob("*.guest.prx"))
                assert [path.name for path in modules] == ["bundled.prx.guest.prx"], (label, name, modules)
                if label == "linux":
                    secondary = output.parent / "picker.picker.bin.elf"
                    assert needed_libraries(modules[0].read_bytes()) == [], needed_libraries(modules[0].read_bytes())
                    assert "libSceVideoOut.prx" in needed_libraries(output.read_bytes())
                    assert "libScePad.prx" in needed_libraries(secondary.read_bytes())

        if os.name != "nt":
            result, output = convert(work / "macos-picker", False, ["--macos", "--executable", "picker/picker.bin"])
            assert result.returncode == 0, (result.stdout, result.stderr)
            for executable in (output, output.parent / "picker.picker.bin.elf"):
                assert executable.read_bytes().startswith(bytes.fromhex("cffaedfe")), executable
                assert stat.S_IMODE(executable.stat().st_mode) & 0o111 == 0o111, (executable, oct(executable.stat().st_mode))


if __name__ == "__main__":
    main()
