from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture, pe_sections
from test_guest_module_directories import module_with_symbol, needed_libraries
from test_guest_needed_modules import executable_with_needed


def pe_export_names(data):
    header, = struct.unpack_from("<I", data, 0x3C)
    export_rva, = struct.unpack_from("<I", data, header + 24 + 112)
    if export_rva == 0:
        return []

    def offset(rva):
        for _, start, size, raw, _ in pe_sections(data):
            if start <= rva < start + size:
                return raw + rva - start
        raise AssertionError(f"Unmapped RVA: {rva:#x}")

    directory = offset(export_rva)
    count, = struct.unpack_from("<I", data, directory + 24)
    names_rva, = struct.unpack_from("<I", data, directory + 32)
    names = offset(names_rva)
    result = []
    for index in range(count):
        name_rva, = struct.unpack_from("<I", data, names + index * 4)
        start = offset(name_rva)
        result.append(data[start:data.index(0, start)].decode())
    return result


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-module-dir-") as directory:
        work = Path(directory)

        def convert(case, windows, options, executable=None, cwd=None, standard=True):
            case.mkdir(parents=True, exist_ok=True)
            if standard:
                (case / "sce_module").mkdir(exist_ok=True)
            source = case / "input.elf"
            source.write_bytes(main_fixture() if executable is None else executable)
            output = case / ("output.exe" if windows else "output.elf")
            result = subprocess.run([str(relinker), *(["--windows"] if windows else []), *options, str(source), str(output)],
                                    cwd=cwd, capture_output=True, text=True, timeout=30)
            return result, output

        for windows in (False, True):
            for relative in (".", "Media/Plugins"):
                case = work / f"{windows}-explicit-only-{relative == '.'}"
                modules = case / relative
                modules.mkdir(parents=True)
                module = modules / "plugin.prx"
                original = bytes(module_with_symbol(True))
                module.write_bytes(original)
                result, output = convert(case, windows, [], standard=False)
                assert result.returncode == 2 and "was not found" in result.stderr, result.stderr
                assert not output.exists()
                result, output = convert(case, windows, ["--module-dir", relative], standard=False)
                assert result.returncode == 0, (result.stdout, result.stderr)
                artifact = case / "app0" / relative / "plugin.prx.guest.prx"
                assert artifact.read_bytes().startswith(b"MZ" if windows else b"\x7fELF")
                assert module.read_bytes() == original
                assert not (case / "sce_module").exists()
                for invalid in ("missing", "../outside"):
                    failed, _ = convert(case, windows, ["--module-dir", invalid], standard=False)
                    assert failed.returncode == 2, (invalid, failed.stdout, failed.stderr)

            case = work / f"{windows}-plugins"
            plugins = case / "Media" / "Plugins"
            plugins.mkdir(parents=True)
            (plugins / "plugin.prx").write_bytes(module_with_symbol(True))
            (plugins / "readme.txt").write_text("not ELF")
            result, output = convert(case, windows, [])
            assert result.returncode == 0, (result.stdout, result.stderr)
            assert not (case / "app0").exists(), list(case.rglob("*.guest.prx"))
            result, output = convert(case, windows, ["--module-dir", "Media/Plugins"])
            assert result.returncode == 0, (result.stdout, result.stderr)
            artifact = case / "app0" / "Media" / "Plugins" / "plugin.prx.guest.prx"
            assert artifact.read_bytes().startswith(b"MZ" if windows else b"\x7fELF"), artifact
            assert list((case / "app0").rglob("*.guest.prx")) == [artifact]
            assert "        |-- Media/\n              |-- Plugins/\n                    |-- plugin.prx.guest.prx\n" in result.stdout, result.stdout
            if windows and os.name == "nt":
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)
            if not windows:
                assert not any("Plugins" in name for name in needed_libraries(output.read_bytes())), needed_libraries(output.read_bytes())
            if windows:
                assert "__aps5_guest_initialize" in pe_export_names(artifact.read_bytes())
                (case / "sce_module" / "bundled.prx").write_bytes(module_with_symbol(True))
                (plugins / "plugin.prx").write_bytes(module_with_symbol(True).replace(b"shared#A#B", b"plugin#A#B"))
                result, output = convert(case, windows, ["--module-dir", "Media/Plugins"])
                assert result.returncode == 0, (result.stdout, result.stderr)
                bundled = case / "app0" / "sce_module" / "bundled.prx.guest.prx"
                assert "__aps5_guest_initialize" not in pe_export_names(bundled.read_bytes())
                assert "__aps5_guest_initialize" in pe_export_names(artifact.read_bytes())
                (case / "sce_module" / "bundled.prx").unlink()

            case = work / f"{windows}-needed"
            modules = case / "Media" / "Modules"
            modules.mkdir(parents=True)
            (modules / "needed.prx").write_bytes(module_with_symbol(True))
            result, output = convert(case, windows, ["--module-dir", "Media/Modules"], executable_with_needed())
            assert result.returncode == 0, (result.stdout, result.stderr)
            needed = case / "app0" / "Media" / "Modules" / "needed.prx.guest.prx"
            if windows:
                assert "__aps5_guest_initialize" not in pe_export_names(needed.read_bytes())
            else:
                assert "$ORIGIN/app0/Media/Modules/needed.prx.guest.prx" in needed_libraries(output.read_bytes()), needed_libraries(output.read_bytes())

            case = work / f"{windows}-root"
            case.mkdir()
            (case / "root.prx").write_bytes(module_with_symbol(True))
            (case / "readme.txt").write_text("not ELF")
            result, output = convert(case, windows, ["--module-dir", "."])
            assert result.returncode == 0, (result.stdout, result.stderr)
            artifact = case / "app0" / "root.prx.guest.prx"
            assert list((case / "app0").rglob("*.guest.prx")) == [artifact], list(case.rglob("*.guest.prx"))
            assert artifact.read_bytes().startswith(b"MZ" if windows else b"\x7fELF"), artifact
            assert "        |-- root.prx.guest.prx\n" in result.stdout, result.stdout
            if windows:
                assert "__aps5_guest_initialize" in pe_export_names(artifact.read_bytes())
            else:
                assert not needed_libraries(output.read_bytes()), needed_libraries(output.read_bytes())
            if windows and os.name == "nt":
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)

            case = work / f"{windows}-root-needed"
            case.mkdir()
            (case / "needed.prx").write_bytes(module_with_symbol(True))
            result, output = convert(case, windows, ["--module-dir", "."], executable_with_needed())
            assert result.returncode == 0, (result.stdout, result.stderr)
            needed = case / "app0" / "needed.prx.guest.prx"
            if windows:
                assert "__aps5_guest_initialize" not in pe_export_names(needed.read_bytes())
            else:
                assert "$ORIGIN/app0/needed.prx.guest.prx" in needed_libraries(output.read_bytes()), needed_libraries(output.read_bytes())

            case = work / f"{windows}-root-executable"
            (case / "picker").mkdir(parents=True)
            (case / "picker" / "picker.bin").write_bytes(main_fixture())
            (case / "root.prx").write_bytes(module_with_symbol(True))
            result, output = convert(case, windows, ["--module-dir", ".", "--executable", "picker/picker.bin"])
            assert result.returncode == 0, (result.stdout, result.stderr)
            picker = case / ("picker.picker.bin" + output.suffix)
            assert picker.read_bytes().startswith(b"MZ" if windows else b"\x7fELF"), picker
            assert list((case / "app0").rglob("*.guest.prx")) == [case / "app0" / "root.prx.guest.prx"], list(case.rglob("*.guest.prx"))

            case = work / f"{windows}-plugins"
            for rejected, message in ((str(plugins), "inside the guest module parent directory"),
                                      ("../outside", "inside the guest module parent directory"),
                                      ("Media/Missing", "not a directory"),
                                      ("sce_module", "Duplicate guest module directory")):
                result, output = convert(case, windows, ["--module-dir", rejected])
                assert result.returncode == 2 and message in result.stderr, (rejected, result.stderr)

            aliases = [["SCE_MODULE"], ["Media/Plugins", "media/plugins"]] if (case / "MEDIA").exists() else []
            if os.name != "nt":
                (case / "linked").symlink_to("sce_module", target_is_directory=True)
                aliases.append(["linked"])
            for names in aliases:
                result, output = convert(case, windows, [option for name in names for option in ("--module-dir", name)])
                assert result.returncode == 2 and "Duplicate guest module directory" in result.stderr, (names, result.stderr)
            if not (case / "MEDIA").exists():
                (case / "SCE_MODULE").mkdir()
                result, output = convert(case, windows, ["--module-dir", "SCE_MODULE"])
                assert result.returncode == 0, (result.stdout, result.stderr)
                (case / "SCE_MODULE").rmdir()

            result, output = convert(case, windows, ["--skip-sce-module", "--module-dir", "Media/Plugins"])
            assert result.returncode != 0 and "conflicts with --skip-sce-module" in result.stderr, result.stderr

            for index, (name, relative) in enumerate((("..plugins", "..plugins"), ("Media/../..plugins", "..plugins"),
                                                       ("./..plugins/.", "..plugins"), ("Media/../Media/Plugins", "Media/Plugins"),
                                                       ("Media/Plugins/", "Media/Plugins"))):
                case = work / f"{windows}-relative-{index}"
                (case / "Media" / "Plugins").mkdir(parents=True)
                (case / "..plugins").mkdir()
                source = case / relative / "plugin.prx"
                original = bytes(module_with_symbol(True))
                source.write_bytes(original)
                result, output = convert(case, windows, ["--module-dir", name])
                assert result.returncode == 0, (name, result.stdout, result.stderr)
                artifact = case / "app0" / relative / "plugin.prx.guest.prx"
                assert list(case.rglob("*.guest.prx")) == [artifact]
                assert artifact.read_bytes().startswith(b"MZ" if windows else b"\x7fELF")
                assert source.read_bytes() == original
                if windows:
                    assert "__aps5_guest_initialize" in pe_export_names(artifact.read_bytes()), name
                else:
                    assert not needed_libraries(output.read_bytes()), name
                if windows and os.name == "nt":
                    run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                    assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)

            case = work / f"{windows}-rooted"
            (case / "Media" / "Plugins").mkdir(parents=True)
            (case / "..plugins").mkdir()
            external = work / f"{windows}-external"
            external.mkdir()
            original = bytes(module_with_symbol(True))
            for directory in (external, case / "..plugins"):
                (directory / "plugin.prx").write_bytes(original)
            rejected = [str(external), "../" + external.name, "Media/../../" + external.name]
            if os.name == "nt":
                rejected.extend((str(external)[len(external.drive):], case.drive + "..plugins", case.drive))
            for name in rejected:
                result, output = convert(case, windows, ["--module-dir", name], cwd=case)
                assert result.returncode == 2 and "inside the guest module parent directory" in result.stderr, (name, result.stderr)
                assert not output.exists() and not list(case.rglob("*.guest.prx"))
                assert (external / "plugin.prx").read_bytes() == original
    print("Guest module directory option tests passed")


if __name__ == "__main__":
    main()
