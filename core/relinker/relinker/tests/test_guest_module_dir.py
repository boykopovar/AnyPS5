from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import elf_loads, main_fixture, pe_sections
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


def with_initializers(image):
    image = bytearray(image)
    tags = [struct.unpack_from("<qQ", image, 0x600 + index * 16) for index in range(8)]
    tags += [(12, 0x1000), (25, 0x2380), (27, 8), (13, 0x1010), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x600 + index * 16, *tag)
    struct.pack_into("<QQ", image, 176 + 32, len(tags) * 16, len(tags) * 16)
    return image


def elf_dynamic(data):
    loads = elf_loads(data)

    def offset(address):
        for _, _, file_offset, mapped, _, file_size, _, _ in loads:
            if mapped <= address < mapped + file_size:
                return file_offset + address - mapped
        raise AssertionError(f"Unmapped address: {address:#x}")

    phoff, = struct.unpack_from("<Q", data, 32)
    size, count = struct.unpack_from("<HH", data, 54)
    dynamic = next(header for header in (struct.unpack_from("<IIQQQQQQ", data, phoff + index * size) for index in range(count))
                   if header[0] == 2)
    tags = {}
    for position in range(dynamic[2], dynamic[2] + dynamic[5], 16):
        tag, value = struct.unpack_from("<qQ", data, position)
        if tag == 0:
            break
        tags[tag] = value
    strings = offset(tags[5])
    _, symbol_count = struct.unpack_from("<II", data, offset(tags[4]))
    symbols = {}
    for index in range(1, symbol_count):
        name, info, _, section, value, symbol_size = struct.unpack_from("<IBBHQQ", data, offset(tags[6]) + index * 24)
        symbols[data[strings + name:data.index(0, strings + name)].decode()] = (info, section, value, symbol_size)
    return tags, symbols, offset


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
            if not windows:
                (plugins / "plugin.prx").write_bytes(with_initializers(module_with_symbol(True)))
                (case / "sce_module" / "bundled.prx").write_bytes(with_initializers(module_with_symbol(True)).replace(b"shared#A#B", b"bundle#A#B"))
                result, output = convert(case, windows, ["--module-dir", "Media/Plugins"])
                assert result.returncode == 0, (result.stdout, result.stderr)
                data = artifact.read_bytes()
                tags, symbols, offset = elf_dynamic(data)
                assert not set(tags) & {12, 13, 25, 26, 27, 28}, sorted(tags)
                info, section, value, size = symbols["__aps5_guest_initialize"]
                assert info == 0x11 and section != 0 and size == 12, (info, section, size)
                assert struct.unpack_from("<III", data, offset(value)) == (0x1000, 1, 0x2380)
                bundled = case / "app0" / "sce_module" / "bundled.prx.guest.prx"
                tags, symbols, _ = elf_dynamic(bundled.read_bytes())
                assert "__aps5_guest_initialize" not in symbols
                assert {12, 13, 25, 27} <= set(tags), sorted(tags)
                (case / "sce_module" / "bundled.prx").unlink()
                (plugins / "plugin.prx").write_bytes(module_with_symbol(True))
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
