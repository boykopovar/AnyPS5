from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture, pe_sections
from test_guest_module_directories import module_with_symbol


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

        def convert(case, windows, options):
            (case / "sce_module").mkdir(parents=True, exist_ok=True)
            source = case / "input.elf"
            source.write_bytes(main_fixture())
            output = case / ("output.exe" if windows else "output.elf")
            result = subprocess.run([str(relinker), *(["--windows"] if windows else []), *options, str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            return result, output

        for windows in (False, True):
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
            assert "    Media/Plugins/plugin.prx.guest.prx\n" in result.stdout, result.stdout
            if windows and os.name == "nt":
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)
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

            for rejected, message in ((str(plugins), "inside the input directory"),
                                      ("../outside", "inside the input directory"),
                                      ("Media/Missing", "not a directory"),
                                      ("sce_module", "Duplicate guest module directory")):
                result, output = convert(case, windows, ["--module-dir", rejected])
                assert result.returncode == 2 and message in result.stderr, (rejected, result.stderr)

            result, output = convert(case, windows, ["--skip-sce-module", "--module-dir", "Media/Plugins"])
            assert result.returncode != 0 and "conflicts with --skip-sce-module" in result.stderr, result.stderr
    print("Guest module directory option tests passed")


if __name__ == "__main__":
    main()
