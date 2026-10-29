from pathlib import Path
import json
import os
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture, pe_sections
from test_guest_module_directories import module_with_symbol, needed_libraries
from test_guest_needed_modules import executable_with_needed, module_with_needed
from test_macos_output import MachO
from test_windows_import_modules import executable, provider


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


def host_module(names=()):
    image = module_with_needed(names, tuple(range(len(names))))
    aligned = bytearray(0x3000)
    aligned[:0x400] = image[:0x400]
    for index in range(3):
        offset = 64 + index * 56
        header = list(struct.unpack_from("<IIQQQQQQ", image, offset))
        aligned[header[3]:header[3] + header[5]] = image[header[2]:header[2] + header[5]]
        header[2] = header[3]
        if header[0] == 1:
            header[7] = 0x1000
        struct.pack_into("<IIQQQQQQ", aligned, offset, *header)
    struct.pack_into("<H", aligned, 56, 4)
    struct.pack_into("<IIQQQQQQ", aligned, 64 + 3 * 56, 0x6474E551, 6, 0, 0, 0, 0, 0, 16)
    return aligned


def check_plugin_host_dependencies(relinker, work):
    for target in ("linux", "macos", "windows"):
        for root in ("runtime", "startup", "required"):
            case = work / f"hosts-{target}-{root}"
            bundled = case / "sce_module"
            plugins = case / "Media" / "Plugins"
            bundled.mkdir(parents=True)
            plugins.mkdir(parents=True)
            (plugins / "plugin.prx").write_bytes(host_module((b"libOptional.prx",)))
            if root == "startup":
                (bundled / "required.prx").write_bytes(host_module((b"libPresent.prx",)))
            image = executable_with_needed(b"plugin.prx") if root == "required" else main_fixture()
            image[0x4000:0x400c] = bytes.fromhex("b83c000000bf2a0000000f05")
            source = case / "input.elf"
            source.write_bytes(image)
            output = case / "output.elf"
            result = subprocess.run([str(relinker), *([] if target == "linux" else [f"--{target}"]),
                                     "--registry", "--skip-syscall-check", "--module-dir", "Media/Plugins",
                                     str(source), str(output)], capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, (target, root, result.stdout, result.stderr)
            plugin = case / "app0" / "Media" / "Plugins" / "plugin.prx.guest.prx"
            required = case / "app0" / "sce_module" / "required.prx.guest.prx"
            expected_hosts = ["libOptional.prx"] if target == "windows" or root == "required" else []
            if root == "startup":
                expected_hosts.append("libPresent.prx")
            assert json.loads((case / "output.needed.json").read_text()) == expected_hosts, (target, root)
            assert json.loads((case / "output.plugin.prx.guest.prx.needed.json").read_text()) == ["libOptional.prx"]
            if target != "windows":
                needed = needed_libraries(output.read_bytes()) if target == "linux" else MachO(output.read_bytes()).dylibs
                guest = ("$ORIGIN/" if target == "linux" else "@executable_path/") + "app0/Media/Plugins/plugin.prx.guest.prx"
                optional = "libOptional.prx" if target == "linux" else "@rpath/libOptional.prx"
                present = "libPresent.prx" if target == "linux" else "@rpath/libPresent.prx"
                assert (guest in needed) == (root == "required"), (target, root, needed)
                assert (optional in needed) == (root == "required"), (target, root, needed)
                plugin_needed = needed_libraries(plugin.read_bytes()) if target == "linux" else MachO(plugin.read_bytes()).dylibs
                assert optional in plugin_needed and present not in plugin_needed, (target, root, plugin_needed)
                if root == "startup":
                    required_needed = needed_libraries(required.read_bytes()) if target == "linux" else MachO(required.read_bytes()).dylibs
                    assert present in needed and present in required_needed and optional not in required_needed, (target, needed, required_needed)
            if target == "linux" and sys.platform == "linux" and os.uname().machine == "x86_64":
                libraries = case / "libs"
                libraries.mkdir()
                (libraries / "libPresent.prx").write_bytes(host_module())
                output.chmod(0o755)
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                if root == "required":
                    assert run.returncode != 0 and "libOptional.prx" in run.stderr, (root, run.returncode, run.stderr)
                else:
                    assert run.returncode == 42, (root, run.returncode, run.stderr)
                command = [sys.executable, "-c", "import ctypes,sys; ctypes.CDLL(sys.argv[1])", str(plugin)]
                load = subprocess.run(command, capture_output=True, text=True, timeout=30)
                assert load.returncode != 0 and "libOptional.prx" in load.stderr, (root, load.returncode, load.stderr)
                (libraries / "libOptional.prx").write_bytes(host_module())
                load = subprocess.run(command, capture_output=True, text=True, timeout=30)
                assert load.returncode == 0, (root, load.returncode, load.stderr)
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                assert run.returncode == 42, (root, run.returncode, run.stderr)
                if root == "startup":
                    (libraries / "libPresent.prx").unlink()
                    run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                    assert run.returncode != 0 and "libPresent.prx" in run.stderr, (run.returncode, run.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-module-dir-") as directory:
        work = Path(directory)
        check_plugin_host_dependencies(relinker, work)

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
                assert "__aps5_guest_initialize" in pe_export_names(needed.read_bytes())
                if os.name == "nt":
                    run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                    assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)
            else:
                assert "$ORIGIN/app0/Media/Modules/needed.prx.guest.prx" in needed_libraries(output.read_bytes()), needed_libraries(output.read_bytes())

            for directory, deferred in (("Media/Modules", True), ("sce_module", False)):
                case = work / f"{windows}-imported-{deferred}"
                (case / directory).mkdir(parents=True)
                (case / directory / "needed.prx").write_bytes(provider(42))
                result, output = convert(case, windows, ["--module-dir", directory] if deferred else [], executable("needed.prx"))
                assert result.returncode == 0, (result.stdout, result.stderr)
                line = f"Deferred module app0/{directory}/needed.prx.guest.prx: the executable imports from it, but it starts only when the title loads it\n"
                if windows:
                    assert (line.encode() in output.read_bytes()) == deferred, directory
                    if os.name == "nt":
                        run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                        assert run.returncode == 42 and (line in run.stdout) == deferred, (directory, run.returncode, run.stdout, run.stderr)
                else:
                    assert f"$ORIGIN/app0/{directory}/needed.prx.guest.prx" in needed_libraries(output.read_bytes()), needed_libraries(output.read_bytes())

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

            case = work / f"{windows}-root-input-link"
            case.mkdir()
            (case / "root.prx").write_bytes(module_with_symbol(True))
            (case / "input.elf").write_bytes(main_fixture())
            os.link(case / "input.elf", case / "input-link.exe")
            result, output = convert(case, windows, ["--module-dir", "."])
            assert result.returncode == 0, (result.stdout, result.stderr)
            assert list((case / "app0").rglob("*.guest.prx")) == [case / "app0" / "root.prx.guest.prx"], list(case.rglob("*.guest.prx"))

            case = work / f"{windows}-root-needed"
            case.mkdir()
            (case / "needed.prx").write_bytes(module_with_symbol(True))
            result, output = convert(case, windows, ["--module-dir", "."], executable_with_needed())
            assert result.returncode == 0, (result.stdout, result.stderr)
            needed = case / "app0" / "needed.prx.guest.prx"
            if windows:
                assert "__aps5_guest_initialize" in pe_export_names(needed.read_bytes())
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
